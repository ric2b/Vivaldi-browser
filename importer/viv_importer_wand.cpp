// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include <openssl/crypto.h>
#include <openssl/digest.h>
#include <openssl/evp.h>

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "components/password_manager/core/browser/password_form.h"
#include "importer/viv_importer.h"
#include "importer/viv_importer_utils.h"

/*
Opera SSL Private Key Verification
Opera Email Password Verification
Opera SSL Password Verification

EVP_BytesToKey(EVP_des_ede3_cbc(), EVP_sha1(), record_salt,
    (const unsigned char *) password, sizeof(password)-1, 6300, key,iv);
*/

/*
Wand format
<00000006> Probable version
28*<00> unknown data
<00000001> tag?
<record len><saltlen><salt><datalen><data> Record : description?
  data: little-endian UTF16 text, null terminated
<01> flag?
<00000001> tag?
<00000000> length?
<record len><saltlen><salt><datalen><data> Record: GUID?
  data: little-endian UTF16 text, null terminated
<record len><saltlen><salt><datalen><data>
    Record: expiration date? 1970-01-01T00:00:00Z -> time_t, 0: never?
  data: little-endian UTF16 text, null terminated

  ---------------

  <0000>
  <guid>
  <submit button><fieldvalue>
  <domain>
  <24*flags>
  <forfield count>
  [<byte><fieldname><fieldvalue><passvalue>]



*/

typedef std::string binary_string;

unsigned char OperaObfuscationPass[] = {0x83, 0x7D, 0xFC, 0x0F, 0x8E, 0xB3,
                                        0xE8, 0x69, 0x73, 0xAF, 0xFF};

char temporary_security_password[] = "foobar1.";

#define TAG_MSB 0x80000000

bool DecryptMasterPasswordKeys(const std::string& password,
                               const std::string& salt,
                               const std::string& in_data,
                               std::string* out_data) {
  uint8_t buffer[EVP_MAX_MD_SIZE];
  unsigned int len = 0;
  unsigned int count = 0;
  unsigned char key[24];
  unsigned char iv[8];

  out_data->clear();

  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);

  EVP_DigestInit(&ctx, EVP_md5());
  EVP_DigestUpdate(&ctx, password.c_str(), password.size());
  EVP_DigestUpdate(&ctx, salt.c_str(), salt.size());
  EVP_DigestFinal(&ctx, buffer, &len);
  DCHECK_EQ(len, 16u);

  memcpy(key, buffer, len);
  count += len;

  EVP_DigestInit(&ctx, EVP_md5());
  EVP_DigestUpdate(&ctx, buffer, len);
  EVP_DigestUpdate(&ctx, password.c_str(), password.size());
  EVP_DigestUpdate(&ctx, salt.c_str(), salt.size());
  EVP_DigestFinal(&ctx, buffer, &len);
  DCHECK_EQ(len, 16u);
  EVP_MD_CTX_cleanup(&ctx);

  memcpy(key + count, buffer, 8);
  memcpy(iv, buffer + 8, 8);

  OPENSSL_cleanse(buffer, sizeof(buffer));

  EVP_CIPHER_CTX decryptor;
  EVP_CipherInit(&decryptor, EVP_des_ede3_cbc(), key, iv, 0);

  OPENSSL_cleanse(key, sizeof(key));
  OPENSSL_cleanse(iv, sizeof(iv));
  out_data->resize(in_data.length());

  int out_len = 0;
  int total_len = 0;
  EVP_CipherUpdate(
      &decryptor,
      reinterpret_cast<uint8_t*>(const_cast<char*>(out_data->data())), &out_len,
      reinterpret_cast<const uint8_t*>(in_data.data()),
      static_cast<unsigned int>(in_data.length()));
  total_len += out_len;
  EVP_CipherFinal_ex(&decryptor,
                     reinterpret_cast<uint8_t*>(
                         const_cast<char*>(out_data->data() + total_len)),
                     &out_len);
  EVP_CIPHER_CTX_cleanup(&decryptor);
  total_len += out_len;
  out_data->resize(total_len);
  OPENSSL_cleanse(iv, sizeof(iv));

  return true;
}

bool ValidatePasswordBlock(const std::string& password,
                           const std::string& salt,
                           const std::string& validation_string,
                           const std::string& in_data) {
  if (salt.length() != 8)
    return false;

  uint8_t buffer[EVP_MAX_MD_SIZE];
  unsigned int len = 0;

  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);

  EVP_DigestInit(&ctx, EVP_sha1());
  EVP_DigestUpdate(&ctx, password.c_str(), password.size());
  EVP_DigestUpdate(&ctx, validation_string.c_str(), validation_string.size());
  EVP_DigestUpdate(&ctx, in_data.c_str(), in_data.size());
  EVP_DigestFinal(&ctx, buffer, &len);
  EVP_MD_CTX_cleanup(&ctx);
  bool ret = (memcmp(salt.c_str(), buffer, 8) == 0);
  OPENSSL_cleanse(buffer, sizeof(buffer));
  return ret;
}

bool WandReadUint32(std::string::iterator* buffer,
                    const std::string::iterator& buffer_end,
                    uint32_t* result) {
  uint32_t temp_result = 0;
  int i;

  for (i = 4; i > 0 && *buffer < buffer_end; i--, (*buffer)++) {
    unsigned char b1 = (unsigned char)**buffer;

    temp_result = (temp_result << 8) | uint32_t(b1);
  }

  if (i > 0)
    return false;

  *result = temp_result;
  return true;
}

bool WandReadUintX(std::string::iterator* buffer,
                   const std::string::iterator& buffer_end,
                   uint32_t int_len,
                   uint32_t* result,
                   bool check_msb = false) {
  uint32_t temp_result = 0;
  uint32_t i;

  if (int_len > 32)
    return false;

  bool msb = false;
  for (i = int_len; i > 0 && *buffer < buffer_end; i--, (*buffer)++) {
    unsigned char b1 = (unsigned char)**buffer;
    if (check_msb && i == int_len) {
      // Get MSB and store it
      msb = ((b1 & 0x80) == 0x80);
      b1 &= 0x7F;
    }

    temp_result = (temp_result << 8) | uint32_t(b1);
  }

  if (i > 0)
    return false;

  if (check_msb && msb)
    temp_result |= TAG_MSB;  // Set MSB if it is stored

  *result = temp_result;
  return true;
}

bool WandReadBuffer(std::string::iterator* buffer,
                    const std::string::iterator& buffer_end,
                    unsigned char* target,
                    uint32_t len) {
  for (; len > 0 && *buffer < buffer_end; len--, (*buffer)++) {
    *(target++) = (unsigned char)**buffer;
  }

  return (len == 0);
}

bool WandReadVector(std::string::iterator* buffer,
                    const std::string::iterator& buffer_end,
                    binary_string* target,
                    uint32_t len) {
  target->resize(len);

  binary_string::iterator target_pos = target->begin();

  for (; len > 0 && *buffer < buffer_end; len--, (*buffer)++, target_pos++) {
    *target_pos = (unsigned char)**buffer;
  }

  return (len == 0);
}

bool WandReadTagLenDataVector(std::string::iterator* buffer,
                              const std::string::iterator& buffer_end,
                              uint32_t tag_len,
                              uint32_t len_len,
                              uint32_t* tag,
                              binary_string* target) {
  target->resize(0);
  *tag = 0;

  if (tag_len && !WandReadUintX(buffer, buffer_end, tag_len, tag, true))
    return false;

  if ((*tag & TAG_MSB) != 0)
    return true;

  uint32_t len = 0;
  if (len_len && !WandReadUintX(buffer, buffer_end, len_len, &len))
    return false;

  if (len == 0)
    return true;

  return WandReadVector(buffer, buffer_end, target, len);
}

bool WandReadEncryptedField(std::string::iterator* buffer,
                            const std::string::iterator& buffer_end,
                            std::u16string* result,
                            binary_string* master_password = NULL) {
  uint32_t len = 0;
  uint32_t iv_len = 0;
  uint32_t data_len = 0;
  binary_string iv;
  binary_string in_data;
  binary_string out_data;

  if (!WandReadUint32(buffer, buffer_end, &len))
    return false;
  if (len == 0)
    return true;
  if (len < 8)
    return false;

  if (!WandReadUint32(buffer, buffer_end, &iv_len))
    return false;
  if (iv_len == 0 || !WandReadVector(buffer, buffer_end, &iv, iv_len))
    return false;

  if (!WandReadUint32(buffer, buffer_end, &data_len))
    return false;
  if (len != 8 + iv_len + data_len)
    return false;
  if (data_len == 0 || !WandReadVector(buffer, buffer_end, &in_data, data_len))
    return false;

  if (master_password && !master_password->empty()) {
    if (!DecryptMasterPasswordKeys(*master_password, iv, in_data, &out_data) ||
        !ValidatePasswordBlock(*master_password, iv,
                               "Opera Email Password Verification", out_data))
      return false;
  } else {
    uint8_t digest_buffer[EVP_MAX_MD_SIZE * 2];
    unsigned int digest_used_len = 0;

    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);

    EVP_DigestInit(&ctx, EVP_md5());
    EVP_DigestUpdate(&ctx, OperaObfuscationPass, sizeof(OperaObfuscationPass));
    EVP_DigestUpdate(&ctx, reinterpret_cast<const uint8_t*>(iv.data()),
                     iv.size());
    EVP_DigestFinal(&ctx, digest_buffer, &digest_used_len);

    EVP_DigestInit(&ctx, EVP_md5());
    EVP_DigestUpdate(&ctx, digest_buffer, digest_used_len);
    EVP_DigestUpdate(&ctx, OperaObfuscationPass, sizeof(OperaObfuscationPass));
    EVP_DigestUpdate(&ctx, reinterpret_cast<const uint8_t*>(iv.data()),
                     iv.size());
    EVP_DigestFinal(&ctx, digest_buffer + digest_used_len, &digest_used_len);

    EVP_MD_CTX_cleanup(&ctx);

    EVP_CIPHER_CTX decryptor;
    EVP_CipherInit(&decryptor, EVP_des_ede3_cbc(), digest_buffer,
                   digest_buffer + 24,
                   0);

    OPENSSL_cleanse(digest_buffer, sizeof(digest_buffer));
    out_data.resize(in_data.length());

    int out_len = 0;
    int total_len = 0;
    EVP_CipherUpdate(
        &decryptor,
        reinterpret_cast<uint8_t*>(const_cast<char*>(out_data.data())),
        &out_len, reinterpret_cast<const uint8_t*>(in_data.data()),
        static_cast<unsigned int>(in_data.length()));
    total_len += out_len;
    EVP_CipherFinal_ex(&decryptor,
                       reinterpret_cast<uint8_t*>(
                           const_cast<char*>(out_data.data() + total_len)),
                       &out_len);
    EVP_CIPHER_CTX_cleanup(&decryptor);
    total_len += out_len;
    out_data.resize(total_len);
  }
  result->clear();
  for (binary_string::iterator it = out_data.begin(); it < out_data.end();) {
    unsigned char c01 = *it;
    it++;
    uint16_t b1 = uint16_t(c01);
    if (it >= out_data.end())
      return false;
    unsigned char c02 = *it;
    it++;
    uint16_t b2 = uint16_t(c02);

    uint16_t c1 = b1 | (b2 << 8);

    if (c1 == 0 && it == out_data.end())
      break;

    result->push_back(wchar_t(c1));
  }

  // TODO(yngve): bigendian vs littleendian
  return true;
}

bool WandReadEncryptedNameAndField(std::string::iterator* buffer,
                                   const std::string::iterator& buffer_end,
                                   std::u16string* name,
                                   std::u16string* value,
                                   bool* is_password,
                                   binary_string* master_password = NULL) {
  *is_password = false;
  (*buffer)++;  // Ignore flag byte
  if (*buffer >= buffer_end)
    return false;

  // Read fieldname
  if (!WandReadEncryptedField(buffer, buffer_end, name))
    return false;

  std::u16string temp_val1;
  std::u16string temp_val2;

  if (!WandReadEncryptedField(buffer, buffer_end, &temp_val1))
    return false;
  if (!WandReadEncryptedField(buffer, buffer_end, &temp_val2, master_password))
    return false;

  if (!temp_val1.empty() || temp_val2.empty()) {
    value->swap(temp_val1);
  } else {
    value->swap(temp_val2);
    *is_password = true;
  }
  return true;
}

struct wand_field_entry {
  std::u16string fieldname;
  std::u16string fieldvalue;
  bool is_password;
};

bool OperaImporter::ImportWand_ReadEntryHTML(
    std::string::iterator* buffer,
    const std::string::iterator& buffer_end,
    std::vector<importer::ImportedPasswordForm>* passwords,
    bool ignore_entry) {
  std::u16string guid;
  std::u16string date_used;
  std::u16string url;
  std::u16string submit_button;
  std::u16string submit_value;
  std::u16string domain;
  uint32_t field_count = 0;
  std::vector<wand_field_entry> fields;
  uint32_t dummy = 0;

  if (wand_version_ == 6) {
    if (!WandReadUint32(buffer, buffer_end, &dummy))
      return false;

    if (!WandReadEncryptedField(buffer, buffer_end, &guid))
      return false;

    if (!WandReadEncryptedField(buffer, buffer_end, &date_used))
      return false;
  }

  if (!WandReadEncryptedField(buffer, buffer_end, &url))
    return false;

  int first_pass = -1;
  int first_field = -1;

  if (!WandReadEncryptedField(buffer, buffer_end, &submit_button))
    return false;
  if (!WandReadEncryptedField(buffer, buffer_end, &submit_value))
    return false;

  if (!WandReadEncryptedField(buffer, buffer_end, &domain))
    return false;

  (*buffer) += 24;
  if (*buffer >= buffer_end)
    return false;

  if (!WandReadUint32(buffer, buffer_end, &field_count))
    return false;

  fields.resize(field_count);

  for (uint32_t i = 0; i < field_count; i++) {
    if (!WandReadEncryptedNameAndField(
            buffer, buffer_end, &fields[i].fieldname, &fields[i].fieldvalue,
            &fields[i].is_password, &master_password_block_))
      return false;
    if (first_pass < 0 && fields[i].is_password)
      first_pass = static_cast<int>(i);
    else if (first_field && !fields[i].is_password)
      first_field = static_cast<int>(i);
  }

  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();
  rep.ClearUsername();
  rep.ClearPassword();

  importer::ImportedPasswordForm password;

  password.scheme = importer::ImportedPasswordForm::Scheme::kHtml;

  password.url = GURL(url).ReplaceComponents(rep);
  if (!password.url.is_valid())
    return true;  // Ignore this entry, no URL

  password.signon_realm = password.url.DeprecatedGetOriginAsURL().spec();
  password.blocked_by_user = (field_count == 0);

  if (first_field >= 0) {
    password.username_element = fields[first_field].fieldname;
    password.username_value = fields[first_field].fieldvalue;
  }

  if (first_pass >= 0) {
    password.password_element = fields[first_pass].fieldname;
    password.password_value = fields[first_pass].fieldvalue;
  }

  passwords->push_back(password);

  return true;
}

bool OperaImporter::ImportWand_ReadEntryAuth(
    std::string::iterator* buffer,
    const std::string::iterator& buffer_end,
    std::vector<importer::ImportedPasswordForm>* passwords,
    bool ignore_entry) {
  std::u16string guid;
  std::u16string date_used;
  std::u16string url;
  std::u16string domain;
  uint32_t field_count = 0;
  std::vector<wand_field_entry> fields;
  uint32_t dummy = 0;

  if (wand_version_ == 6) {
    if (!WandReadUint32(buffer, buffer_end, &dummy))
      return false;

    if (!WandReadEncryptedField(buffer, buffer_end, &guid))
      return false;

    if (!WandReadEncryptedField(buffer, buffer_end, &date_used))
      return false;
  }

  if (!WandReadEncryptedField(buffer, buffer_end, &url))
    return false;

  std::string url8;
  url8 = base::UTF16ToUTF8(url);

  int first_pass = -1;
  int first_field = -1;

  bool http_auth = (url8[0] == '*');
  bool mail_url = (url8.compare(0, std::string::npos, "opera:mail") == 0);
  field_count = 2;
  fields.resize(field_count);

  if (!WandReadEncryptedField(buffer, buffer_end, &fields[0].fieldvalue))
    return false;

  if (!WandReadEncryptedField(buffer, buffer_end, &fields[1].fieldvalue,
                              &master_password_block_))
    return false;

  fields[1].is_password = true;
  first_pass = 1;
  first_field = 0;
  if (mail_url) {
    url8.replace(0, 5, "vivaldi");
  }
  if (http_auth)
    url8.erase(url8.begin());

  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();
  rep.ClearUsername();
  rep.ClearPassword();

  importer::ImportedPasswordForm password;

  password.scheme = importer::ImportedPasswordForm::Scheme::kHtml;
  if (http_auth || mail_url)
    password.scheme = importer::ImportedPasswordForm::Scheme::kBasic;

  password.url = GURL(url8).ReplaceComponents(rep);
  password.signon_realm = password.url.DeprecatedGetOriginAsURL().spec();
  password.blocked_by_user = (field_count == 0);

  if (first_field >= 0) {
    password.username_element = fields[first_field].fieldname;
    password.username_value = fields[first_field].fieldvalue;
  }

  if (first_pass >= 0) {
    password.password_element = fields[first_pass].fieldname;
    password.password_value = fields[first_pass].fieldvalue;
  }

  /*
  for (uint32_t i = 0; i < field_count; i++) {
    if (static_cast<int>(i) != first_field &&
        static_cast<int>(i) != first_pass && !fields[first_pass].is_password) {
      password.all_possible_usernames.push_back(
          password_manager::ValueElementPair(fields[first_pass].fieldvalue,
                                             std::u16string()));
    }
  }
  */

  passwords->push_back(password);

  return true;
}

bool OperaImporter::GetMasterPasswordInfo() {
  if (master_password_.empty())
    return true;

  base::FilePath file(masterpassword_filename_);

  if (!base::PathExists(file))
    return false;

  std::string sec_data;
  base::ReadFileToString(file, &sec_data);

  std::string::iterator sec_buffer = sec_data.begin();
  std::string::iterator sec_buffer_end = sec_data.end();

  uint32_t dummy = 0;
  if (!WandReadUint32(&sec_buffer, sec_buffer_end, &dummy) ||
      dummy != 0x00001000)
    return false;
  if (!WandReadUint32(&sec_buffer, sec_buffer_end, &dummy) ||
      dummy < 0x05050000 || dummy >= 0x05060000)
    return false;

  uint32_t tag_len = 0;
  if (!WandReadUintX(&sec_buffer, sec_buffer_end, 2, &tag_len) || tag_len < 1)
    return false;

  uint32_t len_len = 0;
  if (!WandReadUintX(&sec_buffer, sec_buffer_end, 2, &len_len) || tag_len < 1)
    return false;

  uint32_t tag = 0;
  binary_string master_sec_block;

  if (!WandReadTagLenDataVector(&sec_buffer, sec_buffer_end, tag_len, len_len,
                                &tag, &master_sec_block) ||
      tag != 0x04)
    return false;

  binary_string master_sec_salt;
  binary_string master_sec_encrypted;

  sec_buffer = master_sec_block.begin();
  sec_buffer_end = master_sec_block.end();

  while (sec_buffer < sec_buffer_end) {
    binary_string temp;
    if (!WandReadTagLenDataVector(&sec_buffer, sec_buffer_end, tag_len, len_len,
                                  &tag, &temp))
      return false;
    switch (tag) {
      case 0x50:
        if (master_sec_encrypted.length() == 0)
          master_sec_encrypted = temp;
        break;
      case 0x51:
        if (master_sec_salt.length() == 0)
          master_sec_salt = temp;
        break;
    }
  }

  std::string master_password8;
  master_password8 = base::UTF16ToUTF8(master_password_);
  binary_string master_sec_decrypted;

  if (!DecryptMasterPasswordKeys(master_password8, master_sec_salt,
                                 master_sec_encrypted, &master_sec_decrypted) ||
      !ValidatePasswordBlock(master_password8, master_sec_salt,
                             "Opera SSL Password Verification",
                             master_sec_decrypted))
    return false;

  master_password_block_.assign(master_password8);
  master_password_block_.append(master_sec_decrypted);

  return true;
}

namespace {
bool WandFormatError(std::string* error) {
  *error = "Password file can't be read and might be corrupt";
  return false;
}
}  // namespace

bool OperaImporter::ImportWand(std::string* error) {
  if (wandfilename_.empty()) {
    *error = "No notes filename provided.";
    return false;
  }
  if (master_password_required_ && !GetMasterPasswordInfo()) {
    *error = "Master password required but none was supplied.";
    return false;
  }
  base::FilePath file(wandfilename_);

  if (!base::PathExists(file)) {
    *error = "Password (wand) file does not exist.";
    return false;
  }

  std::string wand_data;
  base::ReadFileToString(file, &wand_data);

  std::string::iterator wand_buffer = wand_data.begin();
  std::string::iterator wand_buffer_end = wand_data.end();

  if (!WandReadUint32(&wand_buffer, wand_buffer_end, &wand_version_))
    return WandFormatError(error);

  if (wand_version_ < 5 || wand_version_ > 6)
    return WandFormatError(error);

  std::vector<importer::ImportedPasswordForm> passwords;

  uint32_t masterpass_used = 0;
  if (!WandReadUint32(&wand_buffer, wand_buffer_end, &masterpass_used))
    return WandFormatError(error);
  if (masterpass_used != 0 && !master_password_block_.empty() &&
      !GetMasterPasswordInfo())
    return WandFormatError(error);

  uint32_t dummy;
  std::u16string dummy_str;
  uint32_t entry_count;
  uint32_t i;
  for (i = 0; i < 6; i++) {
    dummy = 0xffffffff;
    if (!WandReadUint32(&wand_buffer, wand_buffer_end, &dummy))
      return WandFormatError(error);
    if (dummy != 0)
      return WandFormatError(error);
  }

  entry_count = 0;
  if (!WandReadUint32(&wand_buffer, wand_buffer_end, &entry_count))
    return WandFormatError(error);
  if (entry_count != 1)
    return WandFormatError(error);
  dummy_str.clear();
  if (!WandReadEncryptedField(&wand_buffer, wand_buffer_end, &dummy_str))
    return WandFormatError(error);

  if (*(wand_buffer++) != 0x01 || wand_buffer >= wand_buffer_end)
    return WandFormatError(error);

  if (!WandReadUint32(&wand_buffer, wand_buffer_end, &entry_count))
    return WandFormatError(error);

  if (entry_count != 1)
    return WandFormatError(error);

  if (!ImportWand_ReadEntryHTML(&wand_buffer, wand_buffer_end, &passwords,
                                true))
    return WandFormatError(error);

  dummy_str.clear();
  if (!WandReadEncryptedField(&wand_buffer, wand_buffer_end, &dummy_str))
    return WandFormatError(error);

  // TODO(yngve): parse older files

  if (*(wand_buffer++) != 0x00 || wand_buffer >= wand_buffer_end)
    return WandFormatError(error);

  if (!WandReadUint32(&wand_buffer, wand_buffer_end, &entry_count))
    return WandFormatError(error);

  for (i = 0; i < entry_count; i++) {
    if (!ImportWand_ReadEntryHTML(&wand_buffer, wand_buffer_end, &passwords)) {
      LOG(ERROR) << "Failed to import password entry " << i;
      return WandFormatError(error);
    }
  }

  entry_count = 0;
  if (!WandReadUint32(&wand_buffer, wand_buffer_end, &entry_count))
    return WandFormatError(error);

  for (i = 0; i < entry_count; i++) {
    if (!ImportWand_ReadEntryAuth(&wand_buffer, wand_buffer_end, &passwords))
      return WandFormatError(error);
  }

  if (!cancelled()) {
    for (std::vector<importer::ImportedPasswordForm>::iterator it =
             passwords.begin();
         it < passwords.end(); it++)
      bridge_->SetPasswordForm(*it);
  }
  return true;
}
