#include "switch.h"

std::vector<PortConfig> parseConfig(const std::string& filename){
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open config file: " + filename);
    }

    std::vector<PortConfig> config;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream input(line);
        PortConfig port{};
        if (!(input >> port.host >> port.mode >> port.vlan >> port.ip) ||
            (port.mode != "access" && port.mode != "trunk") ||
            port.vlan == 0 || port.vlan > 4094) {
            throw std::runtime_error("Invalid config line: " + line);
        }
        config.push_back(port);
    }
    if (file.bad()) {
        throw std::runtime_error("Could not read config file: " + filename);
    }
    return config;
}

Switch::Switch(std::string filename, std::atomic<bool>& continue_running, 
    char**& argv, PacketProcessor dp, uint32_t switch_id){
    this->continue_running = &continue_running;
    std::cout << "Switch starting" << std::endl;

    std::vector<PortConfig> config;
    config = parseConfig(filename);
    ringBuffer<PacketInfo, 256> rb;

    std::vector<pollfd> ports;
    for (size_t i = 0; i < config.size(); ++i) {
        PortMode pm = config[i].mode == "trunk" ? PortMode::Trunk : PortMode::Access;
        std::vector<uint16_t> vlans = {config[i].vlan};
        std::string intf_name = argv[2+i];
        portList.push_back(std::make_unique<Port>(intf_name, pm, vlans));
        if (!portList.back()->is_valid()) {
            std::cerr << "Failed to initialize switch port " << intf_name << std::endl;
        }
        pollfd pfd{};
        pfd.fd = portList.back()->get_fd();
        pfd.events = POLLIN;
        ports.push_back(pfd);
    }
    this->dp = dp;

    std::vector<std::thread> threads;
    for (size_t i = 0; i < portList.size(); ++i) {
        threads.emplace_back(dp, std::ref(portList), std::ref(ports[i]), i, 
        std::ref(mac_table), this->continue_running, std::ref(monitor), std::ref(rb));
    }

    threads.emplace_back(&Switch::manageControlPackets, this, std::ref(rb));
    for (auto& thread : threads) {
        thread.join();
    }
}

void Switch::manageControlPackets(ringBuffer<PacketInfo, 256>& rb){
    using clock = std::chrono::steady_clock;
    std::unordered_map<uint16_t, NeighborEntry> neighbor_table;

    while (continue_running){
        PacketInfo pkt_info;
        auto next_hello = clock::now() + std::chrono::seconds(2);
        bool success = rb.wait_and_pop(pkt_info, continue_running, std::chrono::seconds(2));

        auto now = clock::now();
        if (success) {
            LldpInfo ll_info;
            if (parseLldpFrame(pkt_info.data, ll_info)){
                uint16_t local_port = pkt_info.ingress_port;

                if (ll_info.ttl == 0) {
                    neighbor_table.erase(local_port);
                } else {
                    neighbor_table[local_port] = NeighborEntry{
                        ll_info.switch_id,
                        ll_info.port_id,
                        10,
                        now + std::chrono::seconds(ll_info.ttl)
                    };
                }
            }
            
        }
        else if (now > next_hello){
            std::vector<uint8_t> frame;
            for (int i = 0; i < portList.size(); ++i){
                frame = buildLldpframe(switch_id, i, portList[i]->get_mac(), 6);
                portList[i]->send(frame.data(), frame.size(), 0);
            }
            next_hello = now + std::chrono::seconds(2);
        }
        
        for (auto it = neighbor_table.begin(); it != neighbor_table.end();) {
            if (now >= it->second.expires_at) {
                it = neighbor_table.erase(it);
            } else {
                ++it;
            }
        }
    }
}

std::vector<std::string> Switch::show_ports(){
    std::vector<std::string> p;
    for (int i = 0; i< portList.size(); ++i){
        p.push_back(portList[i]->getInterface());
    }
    return p;
}

std::string Switch::show_vlans(){
    std::map<uint16_t, std::vector<std::string>> vlan_interface_map = {}; 
    std::vector<std::string> trunk_interfaces = {};
    for (int i = 0; i<portList.size(); ++i){
        if (portList[i]->get_mode() == PortMode::Trunk){
            trunk_interfaces.push_back(portList[i]->getInterface());
        }
        else{
            std::unique_ptr<Port>& p = portList[i];
            if (vlan_interface_map.find(p->get_vlan_id()) == vlan_interface_map.end()){
                vlan_interface_map.insert({p->get_vlan_id(), {p->getInterface()}});
            }
            else{
                vlan_interface_map.at(p->get_vlan_id()).push_back(p->getInterface());
            }
        }
    }
    for (std::string intf : trunk_interfaces){
        for (auto it = vlan_interface_map.begin(); it != vlan_interface_map.end(); ++it){
            it->second.push_back(intf);
        }
    }

    std::string output = "";
    for (std::pair<uint16_t, std::vector<std::string>> vlan_pair : vlan_interface_map){
        output += vlan_pair.first;
        output += ": ";
        for (std::string intf : vlan_pair.second){
            output += intf + ", ";
        }
        output += '\n';
    }
    return output;
}

std::vector<uint8_t> Switch::buildLldpframe(uint8_t* switch_id, uint16_t port_id,
                                            uint8_t* src_mac, uint16_t ttl)
{
    std::vector<uint8_t> frame;
    frame.reserve(64);

    frame.insert(frame.end(), {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E});
    frame.insert(frame.end(), src_mac, src_mac+6);
    appendU16(frame, 0x88CC);

    appendU16(frame, TLV_HEADER(1, 5));
    frame.push_back(7);
    frame.insert(frame.end(), switch_id, switch_id + 6);

    appendU16(frame, TLV_HEADER(2, 3));
    frame.push_back(7);
    appendU16(frame, port_id);

    appendU16(frame, TLV_HEADER(3, 2));
    appendU16(frame, ttl);
    appendU16(frame, TLV_HEADER(0, 0));

    frame.resize(60, 0x00);
    return frame;
}

bool Switch::parseLldpFrame(std::vector<uint8_t>& frame, LldpInfo& ll_info){
    if (frame.size() < 16){
        return false;
    }
    uint16_t ethertype = (uint16_t)((frame[12] << 8) | frame[13]);
    if (ethertype != 0x88CC){
        return false;
    }

    int offset = 14;
    while (offset + 2 <= frame.size()){
        int type = frame[offset];
        int length = frame[offset + 1];
        if (type == 0x00 && length == 0x00){
            break;
        }
        if (type == 1){
            memcpy(&ll_info.switch_id, &frame[offset+3], length);
        }
        if (type == 2){
            memcpy(&ll_info.port_id, &frame[offset+2], length);
            ll_info.port_id = ntohs(ll_info.port_id);
        }
        if (type == 3){
            memcpy(&ll_info.ttl, &frame[offset+2], length);
            ll_info.ttl = ntohs(ll_info.ttl);
        }
        offset += 2 + length;
    }
    return true;
}
