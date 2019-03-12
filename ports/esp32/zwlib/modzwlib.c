#include "modzwlib.h"

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/nlr.h"	

#include <sys/time.h>
#include "extmod/utime_mphal.h"

void ZwGetTime(uint32_t *sec, uint16_t *ms) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *sec = tv.tv_sec, *ms = tv.tv_usec;
}

STATIC mp_obj_t zwlib_initialize() {
    ZwTranInit();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(zwlib_initialize_obj, zwlib_initialize);

STATIC mp_obj_t zw_encode_parse(mp_obj_t self) {
    ZwEncode *encode = (ZwEncode *)self;
    mp_obj_t list = mp_obj_new_list(0, NULL);
    
	// mp_obj_list_append(list, mp_obj_new_int(encode->Crypt));
    uint32_t * DevTm = (uint32_t *)encode->Zip.DevTm;
    uint16_t * DevMs = (uint16_t *)encode->Zip.DevMs;
	mp_obj_list_append(list, mp_obj_new_int(*DevTm));
	mp_obj_list_append(list, mp_obj_new_int(*DevMs));
	mp_obj_list_append(list, mp_obj_new_int(*(uint8_t *)(encode->Zip.DevIP)));
	mp_obj_list_append(list, mp_obj_new_int(*(uint8_t *)(encode->Zip.EntID)));
	mp_obj_list_append(list, mp_obj_new_str((const char *)encode->Zip.DevID, sizeof(encode->Zip.DevID)));
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(zw_encode_parse_obj, zw_encode_parse);

STATIC mp_obj_t zw_encode_new(size_t n_args, const mp_obj_t *args) {

    ZwEncode *self = (ZwEncode *)malloc(sizeof(ZwEncode));
    
    if(NULL != self) {
        if (n_args == 4) {
            mp_obj_t Crypt = args[0], EntID = args[1], DevID = args[2], DevIP = args[3];
            
            mp_buffer_info_t bufinfo;
            mp_get_buffer_raise(DevID, &bufinfo, MP_BUFFER_READ);
            
            if (bufinfo.len != sizeof(self->Zip.DevID)) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_Exception, 
                    "DevID needs a bytearray of length %d (%d given)", sizeof(self->Zip.DevID), bufinfo.len));
            }

            ZwEncodeInit(self, mp_obj_get_int(Crypt) & 0xFF, 
                mp_obj_get_int(EntID) & 0xFF, (uint8_t*)bufinfo.buf, mp_obj_get_int(DevIP) & 0xFF, ZwGetTime);
            
            return MP_OBJ_FROM_PTR(self);
        }
        free(self);
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_Exception, 
            "Crypt = args[0](int), EntID = args[1](int), DevID = args[2](bytearray), DevIP = args[3](int) (%d given)", n_args));
    
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(zw_encode_new_obj, 0, 4, zw_encode_new);

STATIC mp_obj_t zw_encode_del(mp_obj_t self) {
    free(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(zw_encode_del_obj, zw_encode_del);

STATIC mp_obj_t zw_decode_parse(mp_obj_t self) {
    ZwDecode *decode = (ZwDecode *)self;
    mp_obj_t list = mp_obj_new_list(0, NULL);
    
	// mp_obj_list_append(list, mp_obj_new_int(decode->Crypt));
    uint32_t * DevTm = (uint32_t *)decode->Zip.DevTm;
    uint16_t * DevMs = (uint16_t *)decode->Zip.DevMs;
	mp_obj_list_append(list, mp_obj_new_int(*DevTm));
	mp_obj_list_append(list, mp_obj_new_int(*DevMs));
	mp_obj_list_append(list, mp_obj_new_int(*(uint8_t *)(decode->Zip.DevIP)));
	mp_obj_list_append(list, mp_obj_new_int(*(uint8_t *)(decode->Zip.EntID)));
	mp_obj_list_append(list, mp_obj_new_str((const char *)decode->Zip.DevID, sizeof(decode->Zip.DevID)));
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(zw_decode_parse_obj, zw_decode_parse);

STATIC mp_obj_t zw_decode_new(mp_obj_t crypt) {

    ZwDecode *self = (ZwDecode *)malloc(sizeof(ZwDecode));
    
    if(NULL != self) {
        ZwDecodeInit(self, mp_obj_get_int(crypt) & 0xFF);
        
        return MP_OBJ_FROM_PTR(self);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(zw_decode_new_obj, zw_decode_new);

STATIC mp_obj_t zw_decode_del(mp_obj_t self) {
    free(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(zw_decode_del_obj, zw_decode_del);

STATIC mp_obj_t zw_encode_command(mp_obj_t self, mp_obj_t cmd) {
    size_t len;
    const char *ptr = mp_obj_str_get_data(cmd, &len);
    if(len > ZwCmdMax) {
        mp_raise_ValueError("cmd len > ZwCmdMax");
    }
    if(len < 1) {
        mp_raise_ValueError("cmd len < 1");
    }
	uint8_t buffer[ZwTranMax] = {0}, buflen = ZwEncodeCommand(self, buffer, len, (uint8_t *)ptr);
    if (buflen != 0) {
        return mp_obj_new_str((const char *)buffer, buflen);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(zw_encode_command_obj, zw_encode_command);

STATIC mp_obj_t zw_encode_collect(mp_obj_t self, mp_obj_t source, mp_obj_t data) {
    size_t srclen;
    const char *src = mp_obj_str_get_data(source, &srclen);
    if(srclen > ZwSourceMax) {
        mp_raise_ValueError("source len > ZwSourceMax");
    }
    if(srclen < 1) {
        mp_raise_ValueError("source len < 1");
    }
    
    size_t dtlen;
    const char *dt = mp_obj_str_get_data(data, &dtlen);
    if(dtlen > ZwDataMax) {
        mp_raise_ValueError("data len > ZwDataMax");
    }
    if(dtlen < 1) {
        mp_raise_ValueError("data len < 1");
    }
    
	uint8_t buffer[ZwTranMax] = {0}, buflen = ZwEncodeCollect(self, buffer, srclen, (uint8_t *)src, dtlen, (uint8_t *)dt);
    if (buflen != 0) {
        return mp_obj_new_str((const char *)buffer, buflen);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(zw_encode_collect_obj, zw_encode_collect);

STATIC mp_obj_t zw_decode_core(mp_obj_t self, mp_obj_t pack) {

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(pack, &bufinfo, MP_BUFFER_READ);
    if(bufinfo.len >= ZwTranMax) {
        mp_raise_ValueError("pack len > ZwTranMax");
    }
    if(bufinfo.len < 1) {
        mp_raise_ValueError("pack len < 1");
    }
	uint8_t *str = ZwDecodeCore(self, (uint8_t *)bufinfo.buf, bufinfo.len);
    if (str != NULL) {
        return mp_obj_new_bytes((uint8_t *)str, strlen((const char *)str));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(zw_decode_core_obj, zw_decode_core);

STATIC const mp_rom_map_elem_t zwlib_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_zwlib) },
    { MP_ROM_QSTR(MP_QSTR___init__), MP_ROM_PTR(&zwlib_initialize_obj) },

    { MP_ROM_QSTR(MP_QSTR_encode_parse), MP_ROM_PTR(&zw_encode_parse_obj) },
    { MP_ROM_QSTR(MP_QSTR_encode_new), MP_ROM_PTR(&zw_encode_new_obj) },
    { MP_ROM_QSTR(MP_QSTR_encode_del), MP_ROM_PTR(&zw_encode_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_encode_command), MP_ROM_PTR(&zw_encode_command_obj) },
    { MP_ROM_QSTR(MP_QSTR_encode_collect), MP_ROM_PTR(&zw_encode_collect_obj) },
    
    { MP_ROM_QSTR(MP_QSTR_decode_parse), MP_ROM_PTR(&zw_decode_parse_obj) },
    { MP_ROM_QSTR(MP_QSTR_decode_new), MP_ROM_PTR(&zw_decode_new_obj) },
    { MP_ROM_QSTR(MP_QSTR_decode_del), MP_ROM_PTR(&zw_decode_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_decode_core), MP_ROM_PTR(&zw_decode_core_obj) },
    
    { MP_ROM_QSTR(MP_QSTR_TYPE_COMMAND), MP_ROM_INT((mp_uint_t)ZwTranTypeCommand)},
    { MP_ROM_QSTR(MP_QSTR_TYPE_COLLECT), MP_ROM_INT((mp_uint_t)ZwTranTypeCollect)},
    { MP_ROM_QSTR(MP_QSTR_TRAN_MAX), MP_ROM_INT((mp_uint_t)ZwTranMax)},
    { MP_ROM_QSTR(MP_QSTR_DATA_MAX), MP_ROM_INT((mp_uint_t)ZwDataMax)},
    { MP_ROM_QSTR(MP_QSTR_SOURCE_MAX), MP_ROM_INT((mp_uint_t)ZwSourceMax)},
};

STATIC MP_DEFINE_CONST_DICT(zwlib_module_globals, zwlib_module_globals_table);

const mp_obj_module_t zwlib_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&zwlib_module_globals,
};

