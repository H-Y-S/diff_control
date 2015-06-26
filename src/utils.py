def hex2signedint(h):
    x = int(x,16);
    if x > 0x7FFFFFFF:
        x -= 0x100000000

    return x
