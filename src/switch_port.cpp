#include "switch_port.h"

Port::Port(std::string interface_name, PortMode pm, std::vector<uint16_t> vlan_ids)
    : interface(interface_name), fd(-1), tx_fd(-1),
        rx_ring(MAP_FAILED), tx_ring(MAP_FAILED), ifindex(0),
        rx_ring_size(0), tx_ring_size(0), valid(false),
        logger("logs/" + interface + "log") {

    mode = pm;
    if (mode != PortMode::Trunk){
        vlan_id = vlan_ids[0]; //if access intf vlan_ids should only pass one vlan id
    }
    else{
        for (int i = 0; i < vlan_ids.size(); ++i){
            this->allowed_vlans.insert(vlan_ids[i]);
        }
    }
    
    ifindex = if_nametoindex(interface.c_str());
    if (ifindex == 0) {
        fail("Could not find interface " + interface);
        return;
    }

    fd = socket(AF_PACKET, SOCK_RAW | SOCK_NONBLOCK, htons(ETH_P_ALL));
    if (fd < 0) {
        fail("Could not open RX packet socket for " + interface);
        return;
    }

    int version = TPACKET_V3;
    if (setsockopt(fd, SOL_PACKET, PACKET_VERSION, &version, sizeof(version)) < 0) {
        fail("Could not set TPACKET_V3 on " + interface);
        return;
    }

    int ignore_outgoing = 1;
    if (setsockopt(fd, SOL_PACKET, PACKET_IGNORE_OUTGOING, &ignore_outgoing, sizeof(ignore_outgoing)) < 0) {
        fail("Could not disable outgoing packet capture on " + interface);
        return;
    }

    rx_req.tp_block_size = 1 << 20;
    rx_req.tp_block_nr = 8;
    rx_req.tp_frame_size = 1 << 11;
    rx_req.tp_frame_nr = (rx_req.tp_block_size / rx_req.tp_frame_size) * rx_req.tp_block_nr;
    rx_req.tp_retire_blk_tov = 10;
    rx_ring_size = static_cast<size_t>(rx_req.tp_block_size) * rx_req.tp_block_nr;

    if (setsockopt(fd, SOL_PACKET, PACKET_RX_RING, &rx_req, sizeof(rx_req)) < 0) {
        fail("Could not set TPACKET_V3 RX ring on " + interface);
        return;
    }

    sockaddr_ll rx_address{};
    rx_address.sll_family = AF_PACKET;
    rx_address.sll_protocol = htons(ETH_P_ALL);
    rx_address.sll_ifindex = ifindex;
    if (bind(fd, reinterpret_cast<struct sockaddr*>(&rx_address), sizeof(rx_address)) < 0) {
        fail("Could not bind RX socket to " + interface);
        return;
    }

    rx_ring = mmap(nullptr, rx_ring_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rx_ring == MAP_FAILED) {
        fail("Could not map RX ring on " + interface);
        return;
    }

    tx_fd = socket(AF_PACKET, SOCK_RAW | SOCK_NONBLOCK, htons(ETH_P_ALL));
    if (tx_fd < 0) {
        fail("Could not open TX packet socket for " + interface);
        return;
    }

    version = TPACKET_V2;
    if (setsockopt(tx_fd, SOL_PACKET, PACKET_VERSION, &version, sizeof(version)) < 0) {
        fail("Could not set TPACKET_V2 on TX socket for " + interface);
        return;
    }

    tx_req.tp_block_size = 1 << 20;
    tx_req.tp_block_nr = 8;
    tx_req.tp_frame_size = 1 << 16;
    tx_req.tp_frame_nr = (tx_req.tp_block_size / tx_req.tp_frame_size) * tx_req.tp_block_nr;
    tx_ring_size = static_cast<size_t>(tx_req.tp_block_size) * tx_req.tp_block_nr;

    if (setsockopt(tx_fd, SOL_PACKET, PACKET_TX_RING, &tx_req, sizeof(tx_req)) < 0) {
        fail("Could not set TPACKET_V2 TX ring on " + interface);
        return;
    }

    sockaddr_ll tx_address{};
    tx_address.sll_family = AF_PACKET;
    tx_address.sll_protocol = htons(ETH_P_ALL);
    tx_address.sll_ifindex = ifindex;
    if (bind(tx_fd, reinterpret_cast<struct sockaddr*>(&tx_address), sizeof(tx_address)) < 0) {
        fail("Could not bind TX socket to " + interface);
        return;
    }

    tx_ring = mmap(nullptr, tx_ring_size, PROT_READ | PROT_WRITE, MAP_SHARED, tx_fd, 0);
    if (tx_ring == MAP_FAILED) {
        fail("Could not map TX ring on " + interface);
        return;
    }

    valid = true;
    std::cout << "Listening on " << interface << std::endl;
}

ssize_t Port::get(uint8_t* out_buffer, size_t buffer_size, uint16_t& vlan_id) {
    for (unsigned int i = 0; i < rx_req.tp_block_nr; ++i) {
        size_t block_idx = (rx_block_idx + i) % rx_req.tp_block_nr;
        size_t offset = block_idx * rx_req.tp_block_size;
        char* ring_start = static_cast<char*>(rx_ring);
        auto* block = reinterpret_cast<tpacket_block_desc*>(ring_start + offset);

        if ((block->hdr.bh1.block_status & TP_STATUS_USER) == 0) {
            continue;
        }
        std::atomic_thread_fence(std::memory_order_acquire);

        rx_block_idx = block_idx;
        auto* first_packet = reinterpret_cast<tpacket3_hdr*>(
            reinterpret_cast<char*>(block) + block->hdr.bh1.offset_to_first_pkt
        );
        auto* packet = reinterpret_cast<tpacket3_hdr*>(
            reinterpret_cast<uint8_t*>(first_packet) + rx_offset_next_pkt
        );

        size_t packet_length = packet->tp_snaplen;
        if (packet_length > buffer_size) {
            std::cerr << "Dropped oversized packet on " << interface << " (" << packet_length << " bytes)" << std::endl;
            release_rx_block(block, block_idx);
            return 0;
        }

        memcpy(out_buffer, reinterpret_cast<uint8_t*>(packet) + packet->tp_mac, packet_length);
        bool vlan_valid = packet->tp_status & TP_STATUS_VLAN_VALID;
        uint16_t vlan_tci = packet->hv1.tp_vlan_tci;
        uint16_t vlan_tpid = (packet->tp_status & TP_STATUS_VLAN_TPID_VALID)
            ? packet->hv1.tp_vlan_tpid : 0x8100;

        if (packet->tp_next_offset != 0) {
            rx_offset_next_pkt += packet->tp_next_offset;
        } else {
            release_rx_block(block, block_idx);
        }

        if (vlan_valid) {
            if (packet_length < 14 || buffer_size - packet_length < 4) {
                logger.addEntry("Dropped invalid frame on " + interface + "\n");
                return 0;
            }
            memmove(out_buffer + 16, out_buffer + 12, packet_length - 12);
            vlan_tpid = htons(vlan_tpid);
            vlan_tci = htons(vlan_tci);
            memcpy(out_buffer + 12, &vlan_tpid, 2);
            memcpy(out_buffer + 14, &vlan_tci, 2);
            packet_length += 4;
        }

        bool valid = classify_ingress(out_buffer, packet_length, buffer_size, vlan_id);
        if (!valid){
            logger.addEntry("Dropped invalid frame on " + interface + "\n");
            return 0;
        }
        logger.addEntry(interface + " received packet (" + std::to_string(packet_length) + " bytes)\n");
        return packet_length;
    }

    return 0;
}

int Port::send(uint8_t* packet, size_t length, uint16_t vlan_id) {

    if (!can_egress(vlan_id)) {
        return 1;
    }

    bool tagged = (mode == PortMode::Trunk);
    size_t output_len = length + (tagged ? 4 : 0); //put vlan tag back in
    size_t data_offset = TPACKET2_HDRLEN - sizeof(sockaddr_ll);
    if (output_len < length || output_len > tx_req.tp_frame_size - data_offset) {
        std::cerr << "Packet too large for TX ring on " << interface << " (" << output_len << " bytes)" << std::endl;
        return -1;
    }

    for (unsigned int i = 0; i < tx_req.tp_frame_nr; ++i) {
        size_t frame_idx = (tx_frame_idx + i) % tx_req.tp_frame_nr;
        auto* frame = reinterpret_cast<tpacket2_hdr*>(
            static_cast<uint8_t*>(tx_ring) + frame_idx * tx_req.tp_frame_size
        );

        if (frame->tp_status != TP_STATUS_AVAILABLE) {
            continue;
        }
        std::atomic_thread_fence(std::memory_order_acquire);

        uint8_t* frame_data = reinterpret_cast<uint8_t*>(frame) + data_offset;

        if (tagged){
            memcpy(frame_data, packet, 12);
            uint16_t tpid = htons(0x8100);
            memcpy(frame_data + 12, &tpid, 2);

            uint16_t tag_info = htons(vlan_id & 0x0FFF);
            memcpy(frame_data + 14, &tag_info, 2);
            memcpy(frame_data + 16, packet+12, length -12);

        }
        else{
            memcpy(frame_data, packet, length);

        }
        frame->tp_mac = data_offset;
        frame->tp_net = data_offset + ETH_HLEN;
        frame->tp_len = output_len;
        frame->tp_snaplen = output_len;
        std::atomic_thread_fence(std::memory_order_release);
        frame->tp_status = TP_STATUS_SEND_REQUEST;
        tx_frame_idx = (frame_idx + 1) % tx_req.tp_frame_nr;

        if (::send(tx_fd, nullptr, 0, MSG_DONTWAIT) < 0) {
            std::cerr << "Could not submit TX ring packet on " << interface << ": " << std::strerror(errno) << std::endl;
            return -1;
        }

        logger.addEntry(interface + " sent packet (" + std::to_string(length) + " bytes)\n");
        return 0;
    }

    std::cerr << "No available TX ring frame on " << interface << std::endl;
    return -1;
}

int Port::get_fd() const {
    return fd;
}

bool Port::has_pending_packet() const {
    for (unsigned int i = 0; i < rx_req.tp_block_nr; ++i) {
        size_t block_idx = (rx_block_idx + i) % rx_req.tp_block_nr;
        size_t offset = block_idx * rx_req.tp_block_size;
        auto* block = reinterpret_cast<tpacket_block_desc*>(
            static_cast<char*>(rx_ring) + offset
        );
        if ((block->hdr.bh1.block_status & TP_STATUS_USER) != 0) {
            return true;
        }
    }

    return false;
}

bool Port::is_valid() const {
    return valid;
}

Port::~Port() {
    release();
}

bool Port::classify_ingress(uint8_t* pkt, size_t& len, size_t buffer_size, uint16_t& vlan_id){
    if (len < 14){
        return false; //baby frame
    }
    uint16_t ether_type;
    memcpy(&ether_type, pkt + 12, sizeof(ether_type));
    ether_type = ntohs(ether_type);

    if (mode == PortMode::Access){
        if (ether_type == 0x8100){
            return false;
        }
        vlan_id = this->vlan_id;
        return true;
    }
    //PortMode::Trunk
    if (ether_type != 0x8100 || len < 18){
        return false;
    }

    uint16_t tag_info;
    memcpy(&tag_info, pkt+14, sizeof(tag_info));
    tag_info = ntohs(tag_info);

    uint16_t recieved_vlan = tag_info & 0x0FFF;
    if (recieved_vlan == 0 || recieved_vlan == 4095){
        return false;
    }

    if (allowed_vlans.find(recieved_vlan) == allowed_vlans.end()){
        return false;
    }
    memmove(pkt + 12, pkt + 16, len - 16);
    len -= 4;
    vlan_id = recieved_vlan;
    return true;
}

bool Port::can_egress(uint16_t vlan_id) {
    if (mode == PortMode::Access) {
        return vlan_id == this->vlan_id;
    }
    return (allowed_vlans.find(vlan_id) != allowed_vlans.end());

}

void Port::fail(const std::string& message) {
    std::cerr << message << ": " << std::strerror(errno) << std::endl;
    release();
}

void Port::release_rx_block(tpacket_block_desc* block, size_t block_idx) {
    rx_offset_next_pkt = 0;
    std::atomic_thread_fence(std::memory_order_release);
    block->hdr.bh1.block_status = TP_STATUS_KERNEL;
    rx_block_idx = (block_idx + 1) % rx_req.tp_block_nr;
}

void Port::release() {
    if (rx_ring != MAP_FAILED && rx_ring != nullptr) {
        munmap(rx_ring, rx_ring_size);
        rx_ring = MAP_FAILED;
    }
    if (tx_ring != MAP_FAILED && tx_ring != nullptr) {
        munmap(tx_ring, tx_ring_size);
        tx_ring = MAP_FAILED;
    }
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    if (tx_fd >= 0) {
        close(tx_fd);
        tx_fd = -1;
    }
}
