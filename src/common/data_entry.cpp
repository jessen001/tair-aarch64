/*
 * (C) 2007-2017 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * See the AUTHORS file for names of contributors.
 *
 */

#include "data_entry.hpp"
#include "databuffer.hpp"

#ifdef WITH_COMPRESS
#include "compressor.hpp"
#include "define.hpp"

int tair::common::data_entry::compress_type = TAIR_SNAPPY_COMPRESS;
int tair::common::data_entry::compress_threshold = TAIR_DEFAULT_COMPRESS_THRESHOLD;
#endif

namespace tair {
namespace common {
#ifdef WITH_COMPRESS
void data_entry::do_compress(DataBuffer *output) const
{
  int new_size = size;
  char *new_data = get_data();
  uint16_t compress_flag = 0;

  if (get_size() > compress_threshold) {
    //do compress process
    char *dest = NULL;
    uint32_t dest_len = 0, src_len = get_size();
    int compress_ret = compressor::do_compress(&dest, &dest_len, get_data(), src_len, compress_type);
    // to check if the ret is valid
    if (0 == compress_ret) {
      new_size = dest_len;
      new_data = dest;
      compress_flag = compress_type;
      compress_flag <<= COMPRESS_TYPE_OFFSET;
      compress_flag |= COMPRESS_FLAG;
    } else {
      log_error("compress error or size overflow, use the raw data instead");
    }
  } else {
    log_debug("data too small, no need to compress");
  }

  // as need to pin two bytes before data
  char *tmp = new char[new_size + TAIR_VALUE_HEADER_LENGTH];
  assert(tmp != NULL);

  tmp[0] = (char) (compress_flag & 0xFF);
  tmp[1] = (char) ((compress_flag >> 8) & 0xFF);

  memcpy(tmp + TAIR_VALUE_HEADER_LENGTH, new_data, new_size);

  if ((0 != compress_flag) && (NULL != new_data)) {
    delete new_data;
  }

  new_data = tmp;
  new_size += TAIR_VALUE_HEADER_LENGTH;

  data_meta.encode(output, true);

  uint32_t msize = (new_size | (prefix_size << PREFIX_KEY_OFFSET));
  output->writeInt32(msize);
  if (get_size() > 0) {
    output->writeBytes(new_data, new_size);
  }

  // if client side, the new_data is alloc while pin the extra
  // two bytes, delete it after encode process
  if (NULL != new_data)
    delete new_data;
}

bool data_entry::do_decompress()
{
  uint16_t compress_header = *(uint16_t *)get_data();
  bool ret = true;

  // data in server side and generated by old client need not to decompress
  if (0 != (data_meta.flag & TAIR_ITEM_FLAG_COMPRESS)) {
    if (0 == (compress_header & COMPRESS_FLAG)) {
      log_debug("uncompressed data");
      int tmp_size = size - TAIR_VALUE_HEADER_LENGTH;
      char *tmp_data = new char[tmp_size];
      memcpy(tmp_data, data + TAIR_VALUE_HEADER_LENGTH, tmp_size);
      set_alloced_data(tmp_data, tmp_size);
    } else {
      log_debug("compressed data with header: %d", compress_header);
      // do the real compress
      int type = compress_header >> COMPRESS_TYPE_OFFSET;
      char *dest = NULL;
      uint32_t dest_len = 0, src_len = size - TAIR_VALUE_HEADER_LENGTH;
      int compress_ret = compressor::do_decompress(&dest, &dest_len, data + TAIR_VALUE_HEADER_LENGTH, src_len, type);
      // to check if the dest_len overflow
      if (0 == compress_ret) {
        set_alloced_data(dest, dest_len);
        log_debug("decompress ok, len is %d", dest_len);
      } else {
        ret = false;
        log_error("detect compressed data, but decompress err, code is %d", compress_ret);
      }
    }
  } else {
    log_debug("data generated by old version client");
  }

  return ret;
}
#endif

void data_entry::encode(DataBuffer *output, bool need_compress) const {
    output->writeInt8(has_merged);
    output->writeInt32(area);
    output->writeInt16(server_flag);

#ifdef WITH_COMPRESS
    if (need_compress) {
      do_compress(output);
    } else
#else
    UNUSED(need_compress);
#endif
    {
        data_meta.encode(output);
        uint32_t msize = (size | (prefix_size << PREFIX_KEY_OFFSET));
        output->writeInt32(msize);
        if (get_size() > 0) {
            output->writeBytes(get_data(), get_size());
        }
    }
}

bool data_entry::decode(DataBuffer *input, bool need_decompress) {
    free_data();

    uint8_t temp_merged = 0;
    if (input->readInt8(&temp_merged) == false) {
        return false;
    }

    int32_t _area = 0;
    if (input->readInt32(&_area) == false) {
        return false;
    }
    if (_area < 0 || _area >= TAIR_MAX_AREA_COUNT) return false;

    uint16_t flag = 0;
    if (input->readInt16(&flag) == false) {
        return false;
    }

    // data_entry meta info:
    // magic       int16     2B
    // checksum    int16     2B
    // keysize     int16     2B
    // version     int16     2B
    // prefixsize  int32     4B
    // valsize     int32     4B
    // flag        int8      1B
    // cdate       int32     4B
    // mdate       int32     4B
    // edate       int32     4B
    if (data_meta.decode(input) == false) {
        return false;
    }

    uint32_t msize = 0;
    if (input->readInt32(&msize) == false) {
        return false;
    }

    size = (msize & PREFIX_KEY_MASK);
    prefix_size = (msize >> PREFIX_KEY_OFFSET);
    if (size < 0 || size > TAIR_MAX_DATA_SIZE || prefix_size < 0 ||
        prefix_size > TAIR_MAX_KEY_SIZE || prefix_size > size) {
        log_error("invalid data entry size: size(%d), prefix_size(%d)", size, prefix_size);
        return false;
    }
    if (size > 0) {
        if (size > TAIR_MAX_DATA_SIZE) {
            log_error("data entry too large: 0x%x", size);
            return false;
        }
        set_data(NULL, size, true, temp_merged);
        if (input->readBytes(get_data(), size) == false) {
            return false;
        }
    }

    bool ret = true;
#ifdef WITH_COMPRESS
    if (need_decompress) {
      ret = do_decompress();
    }
#else
    UNUSED(need_decompress);
#endif

    has_merged = temp_merged;
    area = _area;
    server_flag = flag;

    return ret;
}

void value_entry::encode(DataBuffer *output) const {
    d_entry.encode(output);
    output->writeInt16(version);
    output->writeInt32(expire);
}

bool value_entry::decode(DataBuffer *input) {
    if (!d_entry.decode(input)) return false;
    if (!input->readInt16(&version)) return false;
    if (!input->readInt32((uint32_t *) &expire)) return false;

    return true;
}


}
} // tair::common