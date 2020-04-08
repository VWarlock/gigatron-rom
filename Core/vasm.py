
# SYNOPSIS: from vasm import *
#
# This is the (back end for) a minimalistic vCPU assembler in Python.
# Use Python itself as the front end.
#
# vCPU mmenomnics are upper cases, as always.
# Special words:
#       ORG(address)    Start new segment
#       L('Name')       Create a label
#       END(address)    Finish assembly, address is execution address

import sys

_gt1 = []       # [addr, [bytes ...], addr, [bytes ...], ...]
_symbols = {}   # name -> value

def L(name):
  if name in _symbols:
    print('Error: Redefined %s' % repr(name))
    sys.exit(1)
  _symbols[name] = _gt1[-2] + len(_gt1[-1])

def ORG(addr): _gt1.extend((addr, []))

def LDWI(op):  return _emit((0x11, (_lo,op), (_hi,op)))
def LD(op):    return _emit((0x1a, op))
def LDW(op):   return _emit((0x21, op))
def STW(op):   return _emit((0x2b, op))
def BEQ(op):   return _emit((0x35, 0x3f, (_br,op)))
def BGT(op):   return _emit((0x35, 0x4d, (_br,op)))
def BLT(op):   return _emit((0x35, 0x50, (_br,op)))
def BGE(op):   return _emit((0x35, 0x53, (_br,op)))
def BLE(op):   return _emit((0x35, 0x56, (_br,op)))
def LDI(op):   return _emit((0x59, op))
def ST(op):    return _emit((0x5e, op))
def POP():     return _emit((0x63))
def BNE(op):   return _emit((0x35, 0x72, (_br,op)))
def PUSH():    return _emit((0x75))
def LUP(op):   return _emit((0x7f, op))
def ANDI(op):  return _emit((0x82, op))
def ORI(op):   return _emit((0x88, op))
def XORI(op):  return _emit((0x8c, op))
def BRA(op):   return _emit((0x90, (_br,op)))
def INC(op):   return _emit((0x93, op))
def ADDW(op):  return _emit((0x99, op))
def PEEK(op):  return _emit((0xad, op))
def SYS(op):   return _emit((0xb4, op))
def SUBW(op):  return _emit((0xb8, op))
def DEF(op):   return _emit((0xcd, (_br,op)))
def CALL(op):  return _emit((0xcf, op))
def ALLOC(op): return _emit((0xdf, op))
def ADDI(op):  return _emit((0xe3, op))
def SUBI(op):  return _emit((0xe6, op))
def LSLW(op):  return _emit((0xe9, op))
def STLW(op):  return _emit((0xec, op))
def LDLW(op):  return _emit((0xee, op))
def POKE(op):  return _emit((0xf0, op))
def DOKE(op):  return _emit((0xf3, op))
def DEEK(op):  return _emit((0xf6, op))
def ANDW(op):  return _emit((0xf8, op))
def ORW(op):   return _emit((0xfa, op))
def XORW(op):  return _emit((0xfc, op))
def RET():     return _emit((0xff))

def _emit(ins):
  _gt1[-1].extend(ins)
  return 0

def END(start):
  with open('out.gt1', 'wb') as fp:
    for ix in range(0, len(_gt1), 2):
      address, segment = _gt1[ix:ix+2]
      if len(segment) == 0:
        continue
      if address + len(segment) > (address | 255) + 1:
        print('Error: Page overrun in segment 0x%04X' % address)
        sys.exit(1)
      segment = [_toByte(x) for x in segment]
      segment = [address >> 8, address & 255, len(segment) & 255] + segment
      fp.write(bytes(segment))
    fp.write(bytes([0, start >> 8, start & 255]))

def _lo(x): return x & 255
def _hi(x): return x >> 8
def _br(x): return (x - 2) & 255

def _toByte(x):
  fn = lambda x: x
  if isinstance(x, tuple):
    fn, x = x[0], x[1]
  if isinstance(x, str):
    if x not in _symbols:
      print('Error: Undefined %s' % repr(x))
      sys.exit(1)
    x = _symbols[x]
  return fn(x)
