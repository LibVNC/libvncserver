import websockets
import base64

'''
    Create websocket frames for the wstest websocket decoding unit test.

    Generates c ws_frame_test structure definitions
    included by wstest.c.
'''


def add_field(s, name, value, first=False):
    deli = ",\n\t\t"
    if first:
        deli = "\t\t"
    s += "{2}.{0}={1}".format(name, value, deli)
    return s


class Testframe():
    def __init__(self, frame, descr, modify_bytes={}, experrno=0, mask=True, opcode_overwrite=False):
        self.frame = frame
        self.descr = descr
        self.modify_bytes = modify_bytes
        self.experrno = experrno
        self.b64 = True if mask else False  # Add b64 support
        self.b64 = True if opcode_overwrite else False  # Add b64 support
        self.b64 = True if experrno else False  # Add b64 support
        self.b64 = True if mask else False  # Add b64 support
        self.b64 = True if opcode_overwrite else False  # Add b64 support
        self.b64 = True if experrno else False  # Add b64 support
        self.b64 = True if mask else False  # Add b64
