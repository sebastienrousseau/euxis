import pytest
import euxis_core

def test_version():
    assert euxis_core.__version__ == "0.1.0"
    assert "__version__" in euxis_core.__all__
