// +-------------------------------------------------------------------------
// | Copyright (C) 2017 Yunify, Inc.
// +-------------------------------------------------------------------------
// | Licensed under the Apache License, Version 2.0 (the "License");
// | You may not use this work except in compliance with the License.
// | You may obtain a copy of the License in the LICENSE file, or at:
// |
// | http://www.apache.org/licenses/LICENSE-2.0
// |
// | Unless required by applicable law or agreed to in writing, software
// | distributed under the License is distributed on an "AS IS" BASIS,
// | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// | See the License for the specific language governing permissions and
// | limitations under the License.
// +-------------------------------------------------------------------------

#include "filesystem/MimeTypes.h"

#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "boost/bind.hpp"
#include "boost/thread/once.hpp"

#include "base/LogMacros.h"

namespace QS {

namespace FileSystem {

using std::string;


static const char *CONTENT_TYPE_STREAM1 = "application/octet-stream";
// static const char *CONTENT_TYPE_STREAM2 = "binary/octet-stream";
static const char *CONTENT_TYPE_DIR = "application/x-directory";
static const char *CONTENT_TYPE_TXT = "text/plain";
// Simulate a symbolic link mime type
static const char *CONTENT_TYPE_SYMLINK = "application/symlink";


static boost::once_flag initOnceFlag;

// --------------------------------------------------------------------------
void InitializeMimeTypes(const std::string &mimeFile) {
  MimeTypes &instance = MimeTypes::Instance();
  boost::call_once(
      initOnceFlag,
      bind(boost::type<void>(), &MimeTypes::Initialize, &instance, mimeFile));
}

// --------------------------------------------------------------------------
string MimeTypes::Find(const string &ext) {
  ExtToMimetypeMapIterator it = m_extToMimeTypeMap.find(ext);
  return it != m_extToMimeTypeMap.end() ? it->second : string();
}

// --------------------------------------------------------------------------
void MimeTypes::Initialize(const std::string &mimeFile) {
  if (mimeFile.empty()) {
    DoDefaultInitialize();
    return;
  }
  std::ifstream file(mimeFile.c_str());
  if (!file) {
    Info("Unable to open file " + mimeFile);
    DoDefaultInitialize();
    return;
  }

  string line;
  while (getline(file, line)) {
    if (line.empty()) continue;
    if (line[0] == '#') continue;

    std::stringstream ss(line);
    string mimeType;
    ss >> mimeType;
    while (ss) {
      string ext;
      ss >> ext;
      if (ext.empty()) continue;
      m_extToMimeTypeMap[ext] = mimeType;
    }
  }
}

// --------------------------------------------------------------------------
void MimeTypes::DoDefaultInitialize() {
  // We just hard code some mime types here by default
  m_extToMimeTypeMap["otf"] = "application/font-sfnt";
  m_extToMimeTypeMap["ttf"] = "application/font-sfnt";
  m_extToMimeTypeMap["pfr"] = "application/font-tdpfr";
  m_extToMimeTypeMap["woff"] = "application/font-woff";
  m_extToMimeTypeMap["gz"] = "application/gzip";
  m_extToMimeTypeMap["jar"] = "application/java-archive";
  m_extToMimeTypeMap["ser"] = "application/java-serialized-object";
  m_extToMimeTypeMap["class"] = "application/java-vm";
  m_extToMimeTypeMap["js"] = "application/javascript";
  m_extToMimeTypeMap["json"] = "application/json";
  m_extToMimeTypeMap["m3g"] = "application/m3g";
  m_extToMimeTypeMap["hqx"] = "application/mac-binhex40";
  m_extToMimeTypeMap["cpt"] = "application/mac-compactpro";
  m_extToMimeTypeMap["nb"] = "application/mathematica";
  m_extToMimeTypeMap["nbp"] = "application/mathematica";
  m_extToMimeTypeMap["mbox"] = "application/mbox";
  m_extToMimeTypeMap["mdb"] = "application/msaccess";
  m_extToMimeTypeMap["doc"] = "application/msword";
  m_extToMimeTypeMap["dot"] = "application/msword";
  m_extToMimeTypeMap["mxf"] = "application/mxf";
  m_extToMimeTypeMap["bin"] = "application/octet-stream";
  m_extToMimeTypeMap["deploy"] = "application/octet-stream";
  m_extToMimeTypeMap["msu"] = "application/octet-stream";
  m_extToMimeTypeMap["msp"] = "application/octet-stream";
  m_extToMimeTypeMap["oda"] = "application/oda";
  m_extToMimeTypeMap["opf"] = "application/oebps-package+xml";
  m_extToMimeTypeMap["ogx"] = "application/ogg";
  m_extToMimeTypeMap["one"] = "application/onenote";
  m_extToMimeTypeMap["onetoc2"] = "application/onenote";
  m_extToMimeTypeMap["onetmp"] = "application/onenote";
  m_extToMimeTypeMap["onepkg"] = "application/onenote";
  m_extToMimeTypeMap["pdf"] = "application/pdf";
  m_extToMimeTypeMap["pgp"] = "application/pgp-encrypted";
  m_extToMimeTypeMap["key"] = "application/pgp-keys";
  m_extToMimeTypeMap["sig"] = "application/pgp-signature";
  m_extToMimeTypeMap["prf"] = "application/pics-rules";
  m_extToMimeTypeMap["ps"] = "application/postscript";
  m_extToMimeTypeMap["ai"] = "application/postscript";
  m_extToMimeTypeMap["eps"] = "application/postscript";
  m_extToMimeTypeMap["epsi"] = "application/postscript";
  m_extToMimeTypeMap["epsf"] = "application/postscript";
  m_extToMimeTypeMap["eps2"] = "application/postscript";
  m_extToMimeTypeMap["eps3"] = "application/postscript";
  m_extToMimeTypeMap["rar"] = "application/rar";
  m_extToMimeTypeMap["rdf"] = "application/rdf+xml";
  m_extToMimeTypeMap["rtf"] = "application/rtf";
  m_extToMimeTypeMap["smi"] = "application/smil+xml";
  m_extToMimeTypeMap["smil"] = "application/smil+xml";
  m_extToMimeTypeMap["xhtml"] = "application/xhtml+xml";
  m_extToMimeTypeMap["xht"] = "application/xhtml+xml";
  m_extToMimeTypeMap["xml"] = "application/xml";
  m_extToMimeTypeMap["xsd"] = "application/xml";
  m_extToMimeTypeMap["xsl"] = "application/xslt+xml";
  m_extToMimeTypeMap["xslt"] = "application/xslt+xml";
  m_extToMimeTypeMap["xspf"] = "application/xspf+xml";
  m_extToMimeTypeMap["zip"] = "application/zip";
  m_extToMimeTypeMap["deb"] = "application/vnd.debian.binary-package";
  m_extToMimeTypeMap["ddeb"] = "application/vnd.debian.binary-package";
  m_extToMimeTypeMap["udeb"] = "application/vnd.debian.binary-package";
  m_extToMimeTypeMap["sfd"] = "application/vnd.font-fontforge-sfd";
  m_extToMimeTypeMap["kml"] = "application/vnd.google-earth.kml+xml";
  m_extToMimeTypeMap["kmz"] = "application/vnd.google-earth.kmz";
  m_extToMimeTypeMap["xul"] = "application/vnd.mozilla.xul+xml";
  m_extToMimeTypeMap["xls"] = "application/vnd.ms-excel";
  m_extToMimeTypeMap["xlb"] = "application/vnd.ms-excel";
  m_extToMimeTypeMap["xlt"] = "application/vnd.ms-excel";
  m_extToMimeTypeMap["eot"] = "application/vnd.ms-fontobject";
  m_extToMimeTypeMap["thmx"] = "application/vnd.ms-officetheme";
  m_extToMimeTypeMap["cat"] = "application/vnd.ms-pki.seccat";
  m_extToMimeTypeMap["ppt"] = "application/vnd.ms-powerpoint";
  m_extToMimeTypeMap["pps"] = "application/vnd.ms-powerpoint";
  m_extToMimeTypeMap["odc"] = "application/vnd.oasis.opendocument.chart";
  m_extToMimeTypeMap["odb"] = "application/vnd.oasis.opendocument.database";
  m_extToMimeTypeMap["odf"] = "application/vnd.oasis.opendocument.formula";
  m_extToMimeTypeMap["odg"] = "application/vnd.oasis.opendocument.graphics";
  m_extToMimeTypeMap["otg"] =
      "application/vnd.oasis.opendocument.graphics-template";
  m_extToMimeTypeMap["odi"] = "application/vnd.oasis.opendocument.image";
  m_extToMimeTypeMap["odp"] = "application/vnd.oasis.opendocument.presentation";
  m_extToMimeTypeMap["otp"] =
      "application/vnd.oasis.opendocument.presentation-template";
  m_extToMimeTypeMap["ods"] = "application/vnd.oasis.opendocument.spreadsheet";
  m_extToMimeTypeMap["ots"] =
      "application/vnd.oasis.opendocument.spreadsheet-template";
  m_extToMimeTypeMap["odt"] = "application/vnd.oasis.opendocument.text";
  m_extToMimeTypeMap["odm"] = "application/vnd.oasis.opendocument.text-master";
  m_extToMimeTypeMap["ott"] =
      "application/vnd.oasis.opendocument.text-template";
  m_extToMimeTypeMap["oth"] = "application/vnd.oasis.opendocument.text-web";
  m_extToMimeTypeMap["7z"] = "application/x-7z-compressed";
  m_extToMimeTypeMap["dvi"] = "application/x-dvi";
  m_extToMimeTypeMap["pfa"] = "application/x-font";
  m_extToMimeTypeMap["pfb"] = "application/x-font";
  m_extToMimeTypeMap["gsf"] = "application/x-font";
  m_extToMimeTypeMap["hdf"] = "application/x-hdf";
  m_extToMimeTypeMap["hwp"] = "application/x-hwp";
  m_extToMimeTypeMap["ica"] = "application/x-ica";
  m_extToMimeTypeMap["info"] = "application/x-info";
  m_extToMimeTypeMap["isp"] = "application/x-internet-signup";
  m_extToMimeTypeMap["ins"] = "application/x-internet-signup";
  m_extToMimeTypeMap["iii"] = "application/x-iphone";
  m_extToMimeTypeMap["iso"] = "application/x-iso9660-image";
  m_extToMimeTypeMap["jam"] = "application/x-jam";
  m_extToMimeTypeMap["jnlp"] = "application/x-java-jnlp-file";
  m_extToMimeTypeMap["jmz"] = "application/x-jmol";
  m_extToMimeTypeMap["chrt"] = "application/x-kchart";
  m_extToMimeTypeMap["kil"] = "application/x-killustrator";
  m_extToMimeTypeMap["skp"] = "application/x-koan";
  m_extToMimeTypeMap["skd"] = "application/x-koan";
  m_extToMimeTypeMap["skt"] = "application/x-koan";
  m_extToMimeTypeMap["skm"] = "application/x-koan";
  m_extToMimeTypeMap["kpr"] = "application/x-kpresenter";
  m_extToMimeTypeMap["kpt"] = "application/x-kpresenter";
  m_extToMimeTypeMap["ksp"] = "application/x-kspread";
  m_extToMimeTypeMap["kwd"] = "application/x-kword";
  m_extToMimeTypeMap["kwt"] = "application/x-kword";
  m_extToMimeTypeMap["latex"] = "application/x-latex";
  m_extToMimeTypeMap["lha"] = "application/x-lha";
  m_extToMimeTypeMap["lyx"] = "application/x-lyx";
  m_extToMimeTypeMap["lzh"] = "application/x-lzh";
  m_extToMimeTypeMap["lzx"] = "application/x-lzx";
  m_extToMimeTypeMap["frm"] = "application/x-maker";
  m_extToMimeTypeMap["maker"] = "application/x-maker";
  m_extToMimeTypeMap["frame"] = "application/x-maker";
  m_extToMimeTypeMap["fm"] = "application/x-maker";
  m_extToMimeTypeMap["fb"] = "application/x-maker";
  m_extToMimeTypeMap["book"] = "application/x-maker";
  m_extToMimeTypeMap["fbdoc"] = "application/x-maker";
  m_extToMimeTypeMap["mif"] = "application/x-mif";
  m_extToMimeTypeMap["m3u8"] = "application/x-mpegURL";
  m_extToMimeTypeMap["application"] = "application/x-ms-application";
  m_extToMimeTypeMap["manifest"] = "application/x-ms-manifest";
  m_extToMimeTypeMap["wmd"] = "application/x-ms-wmd";
  m_extToMimeTypeMap["wmz"] = "application/x-ms-wmz";
  m_extToMimeTypeMap["com"] = "application/x-msdos-program";
  m_extToMimeTypeMap["exe"] = "application/x-msdos-program";
  m_extToMimeTypeMap["bat"] = "application/x-msdos-program";
  m_extToMimeTypeMap["dll"] = "application/x-msdos-program";
  m_extToMimeTypeMap["msi"] = "application/x-msi";
  m_extToMimeTypeMap["nc"] = "application/x-netcdf";
  m_extToMimeTypeMap["pac"] = "application/x-ns-proxy-autoconfig";
  m_extToMimeTypeMap["nwc"] = "application/x-nwc";
  m_extToMimeTypeMap["o"] = "application/x-object";
  m_extToMimeTypeMap["oza"] = "application/x-oz-application";
  m_extToMimeTypeMap["p7r"] = "application/x-pkcs7-certreqresp";
  m_extToMimeTypeMap["crl"] = "application/x-pkcs7-crl";
  m_extToMimeTypeMap["pyc"] = "application/x-python-code";
  m_extToMimeTypeMap["pyo"] = "application/x-python-code";
  m_extToMimeTypeMap["qgs"] = "application/x-qgis";
  m_extToMimeTypeMap["shp"] = "application/x-qgis";
  m_extToMimeTypeMap["shx"] = "application/x-qgis";
  m_extToMimeTypeMap["qtl"] = "application/x-quicktimeplayer";
  m_extToMimeTypeMap["rdp"] = "application/x-rdp";
  m_extToMimeTypeMap["rpm"] = "application/x-redhat-package-manager";
  m_extToMimeTypeMap["rss"] = "application/x-rss+xml";
  m_extToMimeTypeMap["rb"] = "application/x-ruby";
  m_extToMimeTypeMap["sci"] = "application/x-scilab";
  m_extToMimeTypeMap["sce"] = "application/x-scilab";
  m_extToMimeTypeMap["xcos"] = "application/x-scilab-xcos";
  m_extToMimeTypeMap["sh"] = "application/x-sh";
  m_extToMimeTypeMap["shar"] = "application/x-shar";
  m_extToMimeTypeMap["swf"] = "application/x-shockwave-flash";
  m_extToMimeTypeMap["swfl"] = "application/x-shockwave-flash";
  m_extToMimeTypeMap["scr"] = "application/x-silverlight";
  m_extToMimeTypeMap["sql"] = "application/x-sql";
  m_extToMimeTypeMap["sit"] = "application/x-stuffit";
  m_extToMimeTypeMap["sitx"] = "application/x-stuffit";
  m_extToMimeTypeMap["sv4cpio"] = "application/x-sv4cpio";
  m_extToMimeTypeMap["sv4crc"] = "application/x-sv4crc";
  m_extToMimeTypeMap["tar"] = "application/x-tar";
  m_extToMimeTypeMap["tcl"] = "application/x-tcl";
  m_extToMimeTypeMap["gf"] = "application/x-tex-gf";
  m_extToMimeTypeMap["pk"] = "application/x-tex-pk";
  m_extToMimeTypeMap["texinfo"] = "application/x-texinfo";
  m_extToMimeTypeMap["texi"] = "application/x-texinfo";
  m_extToMimeTypeMap["~"] = "application/x-trash";
  m_extToMimeTypeMap["%"] = "application/x-trash";
  m_extToMimeTypeMap["bak"] = "application/x-trash";
  m_extToMimeTypeMap["old"] = "application/x-trash";
  m_extToMimeTypeMap["sik"] = "application/x-trash";
  m_extToMimeTypeMap["t"] = "application/x-troff";
  m_extToMimeTypeMap["tr"] = "application/x-troff";
  m_extToMimeTypeMap["roff"] = "application/x-troff";
  m_extToMimeTypeMap["man"] = "application/x-troff-man";
  m_extToMimeTypeMap["me"] = "application/x-troff-me";
  m_extToMimeTypeMap["ms"] = "application/x-troff-ms";
  m_extToMimeTypeMap["ustar"] = "application/x-ustar";
  m_extToMimeTypeMap["src"] = "application/x-wais-source";
  m_extToMimeTypeMap["wz"] = "application/x-wingz";
  m_extToMimeTypeMap["crt"] = "application/x-x509-ca-cert";
  m_extToMimeTypeMap["xcf"] = "application/x-xcf";
  m_extToMimeTypeMap["fig"] = "application/x-xfig";
  m_extToMimeTypeMap["xpi"] = "application/x-xpinstall";
  m_extToMimeTypeMap["xz"] = "application/x-xz";
  m_extToMimeTypeMap["amr"] = "audio/amr";
  m_extToMimeTypeMap["awb"] = "audio/amr-wb";
  m_extToMimeTypeMap["axa"] = "audio/annodex";
  m_extToMimeTypeMap["snd"] = "audio/basic";
  m_extToMimeTypeMap["au"] = "audio/basic";
  m_extToMimeTypeMap["csd"] = "audio/csound";
  m_extToMimeTypeMap["orc"] = "audio/csound";
  m_extToMimeTypeMap["sco"] = "audio/csound";
  m_extToMimeTypeMap["flac"] = "audio/flac";
  m_extToMimeTypeMap["mid"] = "audio/midi";
  m_extToMimeTypeMap["midi"] = "audio/midi";
  m_extToMimeTypeMap["kar"] = "audio/midi";
  m_extToMimeTypeMap["mpga"] = "audio/mpeg";
  m_extToMimeTypeMap["mpega"] = "audio/mpeg";
  m_extToMimeTypeMap["mp2"] = "audio/mpeg";
  m_extToMimeTypeMap["mp3"] = "audio/mpeg";
  m_extToMimeTypeMap["m4a"] = "audio/mpeg";
  m_extToMimeTypeMap["m3u"] = "audio/mpegurl";
  m_extToMimeTypeMap["oga"] = "audio/ogg";
  m_extToMimeTypeMap["ogg"] = "audio/ogg";
  m_extToMimeTypeMap["opus"] = "audio/ogg";
  m_extToMimeTypeMap["spx"] = "audio/ogg";
  m_extToMimeTypeMap["sid"] = "audio/prs.sid";
  m_extToMimeTypeMap["aif"] = "audio/x-aiff";
  m_extToMimeTypeMap["aiff"] = "audio/x-aiff";
  m_extToMimeTypeMap["aifc"] = "audio/x-aiff";
  m_extToMimeTypeMap["gsm"] = "audio/x-gsm";
  m_extToMimeTypeMap["m3u"] = "audio/x-mpegurl";
  m_extToMimeTypeMap["wma"] = "audio/x-ms-wma";
  m_extToMimeTypeMap["wax"] = "audio/x-ms-wax";
  m_extToMimeTypeMap["ra"] = "audio/x-pn-realaudio";
  m_extToMimeTypeMap["rm"] = "audio/x-pn-realaudio";
  m_extToMimeTypeMap["ram"] = "audio/x-pn-realaudio";
  m_extToMimeTypeMap["ra"] = "audio/x-realaudio";
  m_extToMimeTypeMap["pls"] = "audio/x-scpls";
  m_extToMimeTypeMap["sd2"] = "audio/x-sd2";
  m_extToMimeTypeMap["wav"] = "audio/x-wav";
  m_extToMimeTypeMap["gif"] = "image/gif";
  m_extToMimeTypeMap["ief"] = "image/ief";
  m_extToMimeTypeMap["jpg2"] = "image/jp2";
  m_extToMimeTypeMap["jp2"] = "image/jp2";
  m_extToMimeTypeMap["jpeg"] = "image/jpeg";
  m_extToMimeTypeMap["jpg"] = "image/jpeg";
  m_extToMimeTypeMap["jpe"] = "image/jpeg";
  m_extToMimeTypeMap["jpm"] = "image/jpm";
  m_extToMimeTypeMap["jpf"] = "image/jpx";
  m_extToMimeTypeMap["jpx"] = "image/jpx";
  m_extToMimeTypeMap["pcx"] = "image/pcx";
  m_extToMimeTypeMap["png"] = "image/png";
  m_extToMimeTypeMap["svg"] = "image/svg+xml";
  m_extToMimeTypeMap["svgz"] = "image/svg+xml";
  m_extToMimeTypeMap["tiff"] = "image/tiff";
  m_extToMimeTypeMap["tif"] = "image/tiff";
  m_extToMimeTypeMap["djv"] = "image/vnd.djvu";
  m_extToMimeTypeMap["djvu"] = "image/vnd.djvu";
  m_extToMimeTypeMap["ico"] = "image/vnd.microsoft.icon";
  m_extToMimeTypeMap["art"] = "image/x-jg";
  m_extToMimeTypeMap["jng"] = "image/x-jng";
  m_extToMimeTypeMap["bmp"] = "image/x-ms-bmp";
  m_extToMimeTypeMap["nef"] = "image/x-nikon-nef";
  m_extToMimeTypeMap["orf"] = "image/x-olympus-orf";
  m_extToMimeTypeMap["psd"] = "image/x-photoshop";
  m_extToMimeTypeMap["pnm"] = "image/x-portable-anymap";
  m_extToMimeTypeMap["pbm"] = "image/x-portable-bitmap";
  m_extToMimeTypeMap["pgm"] = "image/x-portable-graymap";
  m_extToMimeTypeMap["ppm"] = "image/x-portable-pixmap";
  m_extToMimeTypeMap["rgb"] = "image/x-rgb";
  m_extToMimeTypeMap["xbm"] = "image/x-xbitmap";
  m_extToMimeTypeMap["xpm"] = "image/x-xpixmap";
  m_extToMimeTypeMap["xwd"] = "image/x-xwindowdump";
  m_extToMimeTypeMap["eml"] = "message/rfc822";
  m_extToMimeTypeMap["igs"] = "model/iges";
  m_extToMimeTypeMap["iges"] = "model/iges";
  m_extToMimeTypeMap["silo"] = "model/mesh";
  m_extToMimeTypeMap["mesh"] = "model/mesh";
  m_extToMimeTypeMap["msh"] = "model/mesh";
  m_extToMimeTypeMap["appcache"] = "text/cache-manifest";
  m_extToMimeTypeMap["ics"] = "text/calendar";
  m_extToMimeTypeMap["icz"] = "text/calendar";
  m_extToMimeTypeMap["css"] = "text/css";
  m_extToMimeTypeMap["csv"] = "text/csv";
  m_extToMimeTypeMap["html"] = "text/html";
  m_extToMimeTypeMap["htm"] = "text/html";
  m_extToMimeTypeMap["shtml"] = "text/html";
  m_extToMimeTypeMap["asc"] = "text/plain";
  m_extToMimeTypeMap["txt"] = "text/plain";
  m_extToMimeTypeMap["text"] = "text/plain";
  m_extToMimeTypeMap["pot"] = "text/plain";
  m_extToMimeTypeMap["brf"] = "text/plain";
  m_extToMimeTypeMap["srt"] = "text/plain";
  m_extToMimeTypeMap["rtx"] = "text/richtext";
  m_extToMimeTypeMap["bib"] = "text/x-bibtex";
  m_extToMimeTypeMap["boo"] = "text/x-boo";
  m_extToMimeTypeMap["h++"] = "text/x-c++hdr";
  m_extToMimeTypeMap["hpp"] = "text/x-c++hdr";
  m_extToMimeTypeMap["hxx"] = "text/x-c++hdr";
  m_extToMimeTypeMap["hh"] = "text/x-c++hdr";
  m_extToMimeTypeMap["c++"] = "text/x-c++src";
  m_extToMimeTypeMap["cpp"] = "text/x-c++src";
  m_extToMimeTypeMap["cxx"] = "text/x-c++src";
  m_extToMimeTypeMap["cc"] = "text/x-c++src";
  m_extToMimeTypeMap["h"] = "text/x-chdr";
  m_extToMimeTypeMap["htc"] = "text/x-component";
  m_extToMimeTypeMap["csh"] = "text/x-csh";
  m_extToMimeTypeMap["c"] = "text/x-csrc";
  m_extToMimeTypeMap["d"] = "text/x-dsrc";
  m_extToMimeTypeMap["patch"] = "text/x-diff";
  m_extToMimeTypeMap["diff"] = "text/x-diff";
  m_extToMimeTypeMap["hs"] = "text/x-haskell";
  m_extToMimeTypeMap["java"] = "text/x-java";
  m_extToMimeTypeMap["pas"] = "text/x-pascal";
  m_extToMimeTypeMap["p"] = "text/x-pascal";
  m_extToMimeTypeMap["gcd"] = "text/x-pcs-gcd";
  m_extToMimeTypeMap["pm"] = "text/x-perl";
  m_extToMimeTypeMap["pl"] = "text/x-perl";
  m_extToMimeTypeMap["py"] = "text/x-python";
  m_extToMimeTypeMap["scala"] = "text/x-scala";
  m_extToMimeTypeMap["sh"] = "text/x-sh";
  m_extToMimeTypeMap["tcl"] = "text/x-tcl";
  m_extToMimeTypeMap["tk"] = "text/x-tcl";
  m_extToMimeTypeMap["cls"] = "text/x-tex";
  m_extToMimeTypeMap["sty"] = "text/x-tex";
  m_extToMimeTypeMap["ltx"] = "text/x-tex";
  m_extToMimeTypeMap["tex"] = "text/x-tex";
  m_extToMimeTypeMap["mpeg"] = "video/mpeg";
  m_extToMimeTypeMap["mpg"] = "video/mpeg";
  m_extToMimeTypeMap["mpe"] = "video/mpeg";
  m_extToMimeTypeMap["ts"] = "video/MP2T";
  m_extToMimeTypeMap["mp4"] = "video/mp4";
  m_extToMimeTypeMap["flv"] = "video/x-flv";
  m_extToMimeTypeMap["wm"] = "video/x-ms-wm";
  m_extToMimeTypeMap["wmv"] = "video/x-ms-wmv";
  m_extToMimeTypeMap["wmx"] = "video/x-ms-wmx";
  m_extToMimeTypeMap["wvx"] = "video/x-ms-wvx";
  m_extToMimeTypeMap["avi"] = "video/x-msvideo";
}

// --------------------------------------------------------------------------
string LookupMimeType(const string &path) {
  string defaultMimeType(CONTENT_TYPE_STREAM1);

  string::size_type lastPos = path.find_last_of('.');
  if (lastPos == string::npos) return defaultMimeType;

  // Extract the last extension
  string ext = path.substr(1 + lastPos);
  // If the last extension matches a mime type, return it
  string mimeType = MimeTypes::Instance().Find(ext);
  if (!mimeType.empty()) return mimeType;

  // Extract the second to last file exstension
  string::size_type firstPos = path.find_first_of('.');
  if (firstPos == lastPos) return defaultMimeType;  // there isn't a 2nd ext
  string ext2;
  if (firstPos != string::npos && firstPos < lastPos) {
    string prefix = path.substr(0, lastPos);
    // Now get the second to last file extension
    string::size_type nextPos = prefix.find_last_of('.');
    if (nextPos != string::npos) {
      ext2 = prefix.substr(1 + nextPos);
    }
  }
  // If the second extension matches a mime type, return it
  mimeType = MimeTypes::Instance().Find(ext2);
  if (!mimeType.empty()) return mimeType;

  return defaultMimeType;
}

// --------------------------------------------------------------------------
string GetDirectoryMimeType() { return CONTENT_TYPE_DIR; }

// --------------------------------------------------------------------------
string GetTextMimeType() { return CONTENT_TYPE_TXT; }

// --------------------------------------------------------------------------
string GetSymlinkMimeType() {
  // Stimulate a mime type for symlink
  return CONTENT_TYPE_SYMLINK;
}

}  // namespace FileSystem
}  // namespace QS
