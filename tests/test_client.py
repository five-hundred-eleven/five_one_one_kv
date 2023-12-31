import random

import pytest

from five_one_one_kv.exceptions import NotHashableError

from .utils import randobytes


def test_basics(client):
    client["foo"] = b"bar"
    assert client["foo"] == b"bar"
    del client["foo"]
    assert client.get("foo") is None


def test_overwrite(client):
    client["foo"] = b"bar"
    assert client["foo"] == b"bar"
    client["foo"] = b"baz"
    assert client["foo"] == b"baz"
    client["foo"] = b"buffoon"
    assert client["foo"] == b"buffoon"
    del client["foo"]
    assert client.get("foo") is None


@pytest.mark.parametrize(
    ("k",),
    (
        ("mel ott homeruns",),
        (b"buffoon",),
        (1001,),
        (2.35813,),
    ),
)
@pytest.mark.parametrize(
    ("v",),
    (
        ("511",),
        (b"baz",),
        ("dalmations",),
        (21.34,),
        (55,),
        ([1, 1, 2, 3, 5, 8, 13, 21],),
    ),
)
def test_types(client, k, v):
    client[k] = v
    assert client[k] == v
    del client[k]
    assert client.get(k) is None


@pytest.mark.parametrize(
    ("k",),
    (
        (
            [
                1,
                2,
                3,
            ],
        ),
        (True,),
        (False,),
    ),
)
def test_unhashable_types(client, k):
    with pytest.raises(NotHashableError):
        client[k] = "bar"


def test_many(client):
    ops = ("get", "set", "del")
    keysource = ("existing", "nonexisting")

    control = {}

    for _ in range(1024):
        if len(control) < 10:
            op = "set"
        else:
            op = random.choice(ops)
        if len(control) == 0:
            ks = "nonexisting"
        else:
            ks = random.choice(keysource)
        if ks == "existing":
            k = random.choice(list(control.keys()))
        else:
            k = randobytes()
        if op == "set":
            v = randobytes()
            control[k] = v
            client[k] = v
        elif op == "get":
            assert control.get(k) == client.get(k)
        elif op == "del":
            if k in control:
                del control[k]
                del client[k]
            else:
                with pytest.raises(KeyError):
                    del client[k]
    for k in control.keys():
        del client[k]
