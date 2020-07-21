# -*- coding: utf-8 -*-
#
# Helper data classes for lspc

class AttrDict(dict):
    def __getattr__(self, key):
        if key in self:
            return self[key]
        raise AttributeError(key)

    def __setattr__(self, key, value):
        self[key] = value



lsp_part_types_e = AttrDict({
    "T_INST": "inst",
    "T_LABEL": "label",
    "T_ADDR_REF": "addr_ref"
})

class lsp_part_t(AttrDict):
    def __new__(cls):
        inst = super().__new__(cls)
        inst.type = cls.type
        inst.tmp_addr = 0
        return inst
    
    def set(self, **kwargs):
        self.update(kwargs)
        return self
    
    _slots_ = ["type", "tmp_addr"]


class lsp_p_inst_t(lsp_part_t):
    _slots_ = ["bytecode"]
    type = lsp_part_types_e.T_INST


class lsp_p_label_t(lsp_part_t):
    _slots_ = ["name"]
    type = lsp_part_types_e.T_LABEL


class lsp_p_addr_ref_t(lsp_part_t):
    _slots_ = ["inst", "args", "opcode", "dst", "width", "tmp_dst"]
    # dst is initialized to the label's name, which is
    # later changed to the actual address ref
    type = lsp_part_types_e.T_ADDR_REF
