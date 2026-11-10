import pytest
import subprocess
import os
from mininet.log import setLogLevel
from utils import parse_configfile

setLogLevel('critical')


def pytest_addoption(parser):
    parser.addoption("--config", action="store", default="config.txt", help="config file to use")


@pytest.fixture(scope="session", autouse=True)
def network_enviroment():
    subprocess.run(["ip", "link", "add", "swm0", "type", "veth", "peer", "name", "swm1"])
    subprocess.run(["ip", "link", "set", "dev", "swm0", "up"])
    subprocess.run(["ip", "link", "set", "dev", "swm1", "up"])
    
    yield
    subprocess.run(["ip", "link", "delete", "swm0"])


@pytest.fixture(scope="session")
def config(request):
    config_file = request.config.getoption("--config")
    return parse_configfile(config_file)


@pytest.fixture(scope="session")
def filename(request):
    config_file = request.config.getoption("--config")
    config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config")
    return os.path.join(config_dir, config_file)