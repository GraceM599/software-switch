import pytest
import subprocess
import time
import os
import signal

@pytest.fixture(scope="session", autouse=True)
def network_enviroment():
    run("ip link add swm0 type veth peer name swm1")
    run("ip link set dev swm0 up")
    run("ip link set dev swm1 up")
    
    yield
    run("ip link delete swm0")
    