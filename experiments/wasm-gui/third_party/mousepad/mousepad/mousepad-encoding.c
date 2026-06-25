/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-encoding.h>
#include <mousepad/mousepad-settings.h>



struct MousepadEncodingInfo
{
  MousepadEncoding  encoding;
  const gchar      *charset;
  const gchar      *name;
};

static const struct MousepadEncodingInfo encoding_infos[MOUSEPAD_N_ENCODINGS] =
{
  /* none */
  { MOUSEPAD_ENCODING_NONE,         NULL,               NULL },

  /* ASCII */
  { MOUSEPAD_ENCODING_ASCII,        "ANSI_X3.4-1968",   "US-ASCII" },

  /* west european */
  { MOUSEPAD_ENCODING_ISO_8859_14,  "ISO-8859-14",      N_("Celtic") },
  { MOUSEPAD_ENCODING_ISO_8859_7,   "ISO-8859-7",       N_("Greek") },
  { MOUSEPAD_ENCODING_WINDOWS_1253, "WINDOWS-1253",     N_("Greek") },
  { MOUSEPAD_ENCODING_ISO_8859_10,  "ISO-8859-10",      N_("Nordic") },
  { MOUSEPAD_ENCODING_ISO_8859_3,   "ISO-8859-3",       N_("South European") },
  { MOUSEPAD_ENCODING_IBM_850,      "IBM850",           N_("Western") },
  { MOUSEPAD_ENCODING_ISO_8859_1,   "ISO-8859-1",       N_("Western") },
  { MOUSEPAD_ENCODING_ISO_8859_15,  "ISO-8859-15",      N_("Western") },
  { MOUSEPAD_ENCODING_WINDOWS_1252, "WINDOWS-1252",     N_("Western") },

  /* east european */
  { MOUSEPAD_ENCODING_ISO_8859_4,   "ISO-8859-4",       N_("Baltic") },
  { MOUSEPAD_ENCODING_ISO_8859_13,  "ISO-8859-13",      N_("Baltic") },
  { MOUSEPAD_ENCODING_WINDOWS_1257, "WINDOWS-1257",     N_("Baltic") },
  { MOUSEPAD_ENCODING_IBM_852,      "IBM852",           N_("Central European") },
  { MOUSEPAD_ENCODING_ISO_8859_2,   "ISO-8859-2",       N_("Central European") },
  { MOUSEPAD_ENCODING_WINDOWS_1250, "WINDOWS-1250",     N_("Central European") },
  { MOUSEPAD_ENCODING_IBM_855,      "IBM855",           N_("Cyrillic") },
  { MOUSEPAD_ENCODING_ISO_8859_5,   "ISO-8859-5",       N_("Cyrillic") },
  { MOUSEPAD_ENCODING_ISO_IR_111,   "ISO-IR-111",       N_("Cyrillic") },
  { MOUSEPAD_ENCODING_KOI8_R,       "KOI8R",            N_("Cyrillic") },
  { MOUSEPAD_ENCODING_WINDOWS_1251, "WINDOWS-1251",     N_("Cyrillic") },
  { MOUSEPAD_ENCODING_CP_866,       "CP866",            N_("Cyrillic/Russian") },
  { MOUSEPAD_ENCODING_KOI8_U,       "KOI8U",            N_("Cyrillic/Ukrainian") },
  { MOUSEPAD_ENCODING_ISO_8859_16,  "ISO-8859-16",      N_("Romanian") },

  /* middle eastern */
  { MOUSEPAD_ENCODING_IBM_864,      "IBM864",           N_("Arabic") },
  { MOUSEPAD_ENCODING_ISO_8859_6,   "ISO-8859-6",       N_("Arabic") },
  { MOUSEPAD_ENCODING_WINDOWS_1256, "WINDOWS-1256",     N_("Arabic") },
  { MOUSEPAD_ENCODING_IBM_862,      "IBM862",           N_("Hebrew") },
  { MOUSEPAD_ENCODING_ISO_8859_8_I, "ISO-8859-8-I",     N_("Hebrew") },
  { MOUSEPAD_ENCODING_WINDOWS_1255, "WINDOWS-1255",     N_("Hebrew") },
  { MOUSEPAD_ENCODING_ISO_8859_8,   "ISO-8859-8",       N_("Hebrew Visual") },

  /* asian */
  { MOUSEPAD_ENCODING_ARMSCII_8,    "ARMSCII-8",        N_("Armenian") },
  { MOUSEPAD_ENCODING_GEOSTD8,      "GEORGIAN-ACADEMY", N_("Georgian") },
  { MOUSEPAD_ENCODING_TIS_620,      "TIS-620",          N_("Thai") },
  { MOUSEPAD_ENCODING_IBM_857,      "IBM857",           N_("Turkish") },
  { MOUSEPAD_ENCODING_WINDOWS_1254, "WINDOWS-1254",     N_("Turkish") },
  { MOUSEPAD_ENCODING_ISO_8859_9,   "ISO-8859-9",       N_("Turkish") },
  { MOUSEPAD_ENCODING_TCVN,         "TCVN",             N_("Vietnamese") },
  { MOUSEPAD_ENCODING_VISCII,       "VISCII",           N_("Vietnamese") },
  { MOUSEPAD_ENCODING_WINDOWS_1258, "WINDOWS-1258",     N_("Vietnamese") },

  /* unicode */
  { MOUSEPAD_ENCODING_UTF_7,        "UTF-7",            N_("Unicode") },
  { MOUSEPAD_ENCODING_UTF_8,        "UTF-8",            N_("Unicode") },
  { MOUSEPAD_ENCODING_UTF_16LE,     "UTF-16LE",         N_("Unicode") },
  { MOUSEPAD_ENCODING_UTF_16BE,     "UTF-16BE",         N_("Unicode") },
  { MOUSEPAD_ENCODING_UCS_2LE,      "UCS-2LE",          N_("Unicode") },
  { MOUSEPAD_ENCODING_UCS_2BE,      "UCS-2BE",          N_("Unicode") },
  { MOUSEPAD_ENCODING_UTF_32LE,     "UTF-32LE",         N_("Unicode") },
  { MOUSEPAD_ENCODING_UTF_32BE,     "UTF-32BE",         N_("Unicode") },

  /* east asian */
  { MOUSEPAD_ENCODING_GB18030,      "GB18030",          N_("Chinese Simplified") },
  { MOUSEPAD_ENCODING_GB2312,       "GB2312",           N_("Chinese Simplified") },
  { MOUSEPAD_ENCODING_GBK,          "GBK",              N_("Chinese Simplified") },
  { MOUSEPAD_ENCODING_HZ,           "HZ",               N_("Chinese Simplified") },
  { MOUSEPAD_ENCODING_BIG5,         "BIG5",             N_("Chinese Traditional") },
  { MOUSEPAD_ENCODING_BIG5_HKSCS,   "BIG5-HKSCS",       N_("Chinese Traditional") },
  { MOUSEPAD_ENCODING_EUC_TW,       "EUC-TW",           N_("Chinese Traditional") },
  { MOUSEPAD_ENCODING_EUC_JP,       "EUC-JP",           N_("Japanese") },
  { MOUSEPAD_ENCODING_ISO_2022_JP,  "ISO-2022-JP",      N_("Japanese") },
  { MOUSEPAD_ENCODING_SHIFT_JIS,    "SHIFT_JIS",        N_("Japanese") },
  { MOUSEPAD_ENCODING_EUC_KR,       "EUC-KR",           N_("Korean") },
  { MOUSEPAD_ENCODING_ISO_2022_KR,  "ISO-2022-KR",      N_("Korean") },
  { MOUSEPAD_ENCODING_JOHAB,        "JOHAB",            N_("Korean") },
  { MOUSEPAD_ENCODING_UHC,          "UHC",              N_("Korean") }
};



const gchar *
mousepad_encoding_get_charset (MousepadEncoding encoding)
{
  guint i;

  /* try to find and return the charset */
  for (i = 0; i < MOUSEPAD_N_ENCODINGS; i++)
    if (encoding_infos[i].encoding == encoding)
      return encoding_infos[i].charset;

  return NULL;
}



const gchar *
mousepad_encoding_get_name (MousepadEncoding encoding)
{
  guint i;

  /* try to find and return the translated name */
  for (i = 0; i < MOUSEPAD_N_ENCODINGS; i++)
    if (encoding_infos[i].encoding == encoding)
      return i == 0 ? NULL : _(encoding_infos[i].name);

  return NULL;
}



MousepadEncoding
mousepad_encoding_find (const gchar *charset)
{
  MousepadEncoding  encoding = MOUSEPAD_ENCODING_NONE;
  gchar            *up_charset = NULL;
  guint             i;

  /* switch to ASCII upper case */
  if (charset != NULL)
    up_charset = g_ascii_strup (charset, -1);

  /* find the charset */
  for (i = 0; i < MOUSEPAD_N_ENCODINGS; i++)
    if (g_strcmp0 (encoding_infos[i].charset, up_charset) == 0)
      {
        encoding = encoding_infos[i].encoding;
        break;
      }

  /* clenaup */
  g_free (up_charset);

  return encoding;
}



MousepadEncoding
mousepad_encoding_get_default (void)
{
  MousepadEncoding  encoding;
  gchar            *charset;

  charset = MOUSEPAD_SETTING_GET_STRING (DEFAULT_ENCODING);
  encoding = mousepad_encoding_find (charset);

  if (encoding == MOUSEPAD_ENCODING_NONE)
    {
      g_warning ("Invalid encoding '%s': falling back to default UTF-8", charset);
      encoding = MOUSEPAD_ENCODING_UTF_8;
    }

  g_free (charset);

  return encoding;
}



MousepadEncoding
mousepad_encoding_get_system (void)
{
  const gchar *charset;

  g_get_charset (&charset);

  return mousepad_encoding_find (charset);
}



MousepadEncoding
mousepad_encoding_read_bom (const gchar *contents,
                            gsize        length,
                            gsize       *bom_length)
{
  const guchar     *bom = (const guchar *) contents;
  MousepadEncoding  encoding = MOUSEPAD_ENCODING_NONE;
  gsize             bytes = 0;

  g_return_val_if_fail (contents != NULL && length > 0, MOUSEPAD_ENCODING_NONE);
  g_return_val_if_fail (contents != NULL && length > 0, MOUSEPAD_ENCODING_NONE);

  switch (bom[0])
    {
      case 0x2b:
        if (length >= 4 && (bom[1] == 0x2f && bom[2] == 0x76) &&
            (bom[3] == 0x38 || bom[3] == 0x39 || bom[3] == 0x2b || bom[3] == 0x2f))
          {
            bytes = 4;
            encoding = MOUSEPAD_ENCODING_UTF_7;
          }
        break;

      case 0xef:
        if (length >= 3 && bom[1] == 0xbb && bom[2] == 0xbf)
          {
            bytes = 3;
            encoding = MOUSEPAD_ENCODING_UTF_8;
          }
        break;

      case 0xfe:
        if (length >= 2 && bom[1] == 0xff)
          {
            bytes = 2;
            encoding = MOUSEPAD_ENCODING_UTF_16BE;
          }
        break;

      case 0xff:
        if (length >= 4 && bom[1] == 0xfe && bom[2] == 0x00 && bom[3] == 0x00)
          {
            bytes = 4;
            encoding = MOUSEPAD_ENCODING_UTF_32LE;
          }
        else if (length >= 2 && bom[1] == 0xfe)
          {
            bytes = 2;
            encoding = MOUSEPAD_ENCODING_UTF_16LE;
          }
        break;

      case 0x00:
        if (length >= 4 && bom[1] == 0x00 && bom[2] == 0xfe && bom[3] == 0xff)
          {
            bytes = 4;
            encoding = MOUSEPAD_ENCODING_UTF_32BE;
          }
        break;
    }

  if (bom_length != NULL)
    *bom_length = bytes;

  return encoding;
}



void
mousepad_encoding_write_bom (MousepadEncoding  *encoding,
                             gsize             *length,
                             gchar            **contents)
{
  guchar bom[4];
  gsize  bytes = 0, n;

  switch (*encoding)
    {
      /* don't preserve UTF-7 because of its strange BOM but preserve other encodings */
      case MOUSEPAD_ENCODING_UTF_7:
      case MOUSEPAD_ENCODING_UTF_8:
        *encoding = MOUSEPAD_ENCODING_UTF_8;
        bom[0] = 0xef;
        bom[1] = 0xbb;
        bom[2] = 0xbf;
        bytes = 3;
        break;

      case MOUSEPAD_ENCODING_UTF_16BE:
        bom[0] = 0xfe;
        bom[1] = 0xff;
        bytes = 2;
        break;

      case MOUSEPAD_ENCODING_UTF_16LE:
        bom[0] = 0xff;
        bom[1] = 0xfe;
        bytes = 2;
        break;

      case MOUSEPAD_ENCODING_UTF_32BE:
        bom[0] = 0x00;
        bom[1] = 0x00;
        bom[2] = 0xfe;
        bom[3] = 0xff;
        bytes = 4;
        break;

      case MOUSEPAD_ENCODING_UTF_32LE:
        bom[0] = 0xff;
        bom[1] = 0xfe;
        bom[2] = 0x00;
        bom[3] = 0x00;
        bytes = 4;
        break;

      default:
        return;
        break;
    }

  if (G_LIKELY (bytes > 0))
    {
      /* realloc the contents string */
      *contents = g_renew (gchar, *contents, *length + bytes + 1);

      /* move the existing contents */
      memmove (*contents + bytes, *contents, *length + 1);

      /* write the bom at the start of the contents */
      for (n = 0; n < bytes; n++)
        (*contents)[n] = bom[n];

      /* increase the length */
      *length += bytes;
    }
}
