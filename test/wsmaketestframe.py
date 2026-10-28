import base64

class Testframe():
    def __init__(self, frame, descr, modify_bytes={}, experrno=0, mask=True, opcode_overwrite=False):
        self.frame = frame
        self.descr = descr
        self.modify_bytes = modify_bytes
        self.experrno = experrno
        self.b64 = True if mask else False
        self.b64_bytes = base64.b64encode(self.frame).decode('utf-8')
        self.b64_bytes = base64.b64encode(self.b64_bytes).decode('utf-8')
        self.b64_bytes = base64.b64encode(self.b64_bytes).decode('utf-8')
