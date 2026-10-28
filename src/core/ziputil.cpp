#include "ziputil.h"
#include <QFile>
#include <QVector>

namespace ZipUtil {

// ---------- little-endian helpers ----------
static quint16 readLE16(const QByteArray& d, qint64 p) {
    return (quint8)d[p] | ((quint8)d[p + 1] << 8);
}
static quint32 readLE32(const QByteArray& d, qint64 p) {
    return (quint8)d[p] | ((quint8)d[p + 1] << 8) |
           ((quint8)d[p + 2] << 16) | ((quint32)(quint8)d[p + 3] << 24);
}
static void appendLE16(QByteArray& b, quint16 v) {
    b.append((char)(v & 0xFF));
    b.append((char)((v >> 8) & 0xFF));
}
static void appendLE32(QByteArray& b, quint32 v) {
    b.append((char)(v & 0xFF));
    b.append((char)((v >> 8) & 0xFF));
    b.append((char)((v >> 16) & 0xFF));
    b.append((char)((v >> 24) & 0xFF));
}

// ---------- CRC-32 (IEEE 802.3) ----------
static quint32 crc32(const QByteArray& data) {
    static quint32 table[256];
    static bool init = false;
    if (!init) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (char ch : data) {
        crc = table[(crc ^ (quint8)ch) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// ---------- DEFLATE bit reader ----------
struct BitReader {
    const unsigned char* data;
    size_t len;
    size_t bytePos;
    unsigned int bitBuf;
    int bitCnt;
    BitReader(const unsigned char* d, size_t l)
        : data(d), len(l), bytePos(0), bitBuf(0), bitCnt(0) {}
    int getBit() {
        if (bitCnt == 0) {
            if (bytePos >= len) return -1;
            bitBuf = data[bytePos++];
            bitCnt = 8;
        }
        int b = (int)(bitBuf & 1);
        bitBuf >>= 1;
        bitCnt--;
        return b;
    }
    int getBits(int n) {
        int v = 0;
        for (int i = 0; i < n; i++) {
            int b = getBit();
            if (b < 0) return -1;
            v |= (b << i);
        }
        return v;
    }
};

// Build canonical Huffman tables (counts[len] + symbols[] ordered by len then symbol).
static void buildHuff(const unsigned char* cl, int n, int* counts, int* symbols) {
    for (int i = 0; i < 16; i++) counts[i] = 0;
    for (int i = 0; i < n; i++)
        if (cl[i]) counts[cl[i]]++;
    int offsets[16];
    offsets[1] = 0;
    for (int i = 1; i < 15; i++) offsets[i + 1] = offsets[i] + counts[i];
    for (int i = 0; i < n; i++)
        if (cl[i]) symbols[offsets[cl[i]]++] = i;
}

// Count-based symbol decode (puff-style).
static int decodeSymbol(BitReader& br, const int* counts, const int* symbols) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; len++) {
        int b = br.getBit();
        if (b < 0) return -1;
        code |= b;
        int count = counts[len];
        if (code - first < count)
            return symbols[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

// ---------- RFC 1951 inflate ----------
QByteArray inflateRaw(const QByteArray& deflate) {
    static const int lengthBase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,
                                       51,59,67,83,99,115,131,163,195,227,258};
    static const int lengthExtra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const int distBase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,
                                     513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const int distExtra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    static const int order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

    BitReader br((const unsigned char*)deflate.constData(), (size_t)deflate.size());
    QByteArray out;
    bool final = false;

    do {
        int bfinal = br.getBit();
        if (bfinal < 0) return QByteArray();
        final = (bfinal == 1);
        int btype = br.getBits(2);
        if (btype < 0) return QByteArray();

        if (btype == 0) {
            // stored block: align to byte boundary
            br.bitCnt = 0;
            br.bitBuf = 0;
            if (br.bytePos + 4 > br.len) return QByteArray();
            int len = (quint8)br.data[br.bytePos] | ((quint8)br.data[br.bytePos + 1] << 8);
            int nlen = (quint8)br.data[br.bytePos + 2] | ((quint8)br.data[br.bytePos + 3] << 8);
            br.bytePos += 4;
            if ((len ^ 0xFFFF) != nlen) return QByteArray();
            if (br.bytePos + len > br.len) return QByteArray();
            out.append((const char*)br.data + br.bytePos, len);
            br.bytePos += len;
            continue;
        }

        int* litCounts = new int[16]();
        int* litSymbols = new int[288]();
        int* distCounts = new int[16]();
        int* distSymbols = new int[32]();

        auto cleanup = [&]() {
            delete[] litCounts; delete[] litSymbols;
            delete[] distCounts; delete[] distSymbols;
        };

        if (btype == 1) {
            // fixed Huffman
            unsigned char cl[288];
            for (int i = 0; i < 144; i++) cl[i] = 8;
            for (int i = 144; i < 256; i++) cl[i] = 9;
            for (int i = 256; i < 280; i++) cl[i] = 7;
            for (int i = 280; i < 288; i++) cl[i] = 8;
            buildHuff(cl, 288, litCounts, litSymbols);
            unsigned char dcl[30];
            for (int i = 0; i < 30; i++) dcl[i] = 5;
            buildHuff(dcl, 30, distCounts, distSymbols);
        } else if (btype == 2) {
            int hlit = br.getBits(5);
            if (hlit < 0) { cleanup(); return QByteArray(); }
            hlit += 257;
            int hdist = br.getBits(5);
            if (hdist < 0) { cleanup(); return QByteArray(); }
            hdist += 1;
            int hclen = br.getBits(4);
            if (hclen < 0) { cleanup(); return QByteArray(); }
            hclen += 4;
            unsigned char clLen[19] = {0};
            for (int i = 0; i < hclen; i++) {
                int v = br.getBits(3);
                if (v < 0) { cleanup(); return QByteArray(); }
                clLen[order[i]] = (unsigned char)v;
            }
            int clCounts[16] = {0};
            int clSymbols[19] = {0};
            buildHuff(clLen, 19, clCounts, clSymbols);
            unsigned char lengths[288 + 32];
            int n = hlit + hdist;
            int i = 0;
            while (i < n) {
                int sym = decodeSymbol(br, clCounts, clSymbols);
                if (sym < 0) { cleanup(); return QByteArray(); }
                if (sym < 16) {
                    lengths[i++] = (unsigned char)sym;
                } else if (sym == 16) {
                    int rep = br.getBits(2);
                    if (rep < 0) { cleanup(); return QByteArray(); }
                    rep += 3;
                    if (i == 0) { cleanup(); return QByteArray(); }
                    unsigned char prev = lengths[i - 1];
                    while (rep-- > 0 && i < n) lengths[i++] = prev;
                } else if (sym == 17) {
                    int rep = br.getBits(3);
                    if (rep < 0) { cleanup(); return QByteArray(); }
                    rep += 3;
                    while (rep-- > 0 && i < n) lengths[i++] = 0;
                } else if (sym == 18) {
                    int rep = br.getBits(7);
                    if (rep < 0) { cleanup(); return QByteArray(); }
                    rep += 11;
                    while (rep-- > 0 && i < n) lengths[i++] = 0;
                } else {
                    cleanup();
                    return QByteArray();
                }
            }
            buildHuff(lengths, hlit, litCounts, litSymbols);
            buildHuff(lengths + hlit, hdist, distCounts, distSymbols);
        } else {
            cleanup();
            return QByteArray(); // btype == 3 is reserved/error
        }

        while (true) {
            int sym = decodeSymbol(br, litCounts, litSymbols);
            if (sym < 0) { cleanup(); return QByteArray(); }
            if (sym == 256) break;
            if (sym < 256) {
                out.append((char)sym);
            } else {
                sym -= 257;
                if (sym >= 29) { cleanup(); return QByteArray(); }
                int len = lengthBase[sym];
                int extra = br.getBits(lengthExtra[sym]);
                if (extra < 0) { cleanup(); return QByteArray(); }
                len += extra;
                int dsym = decodeSymbol(br, distCounts, distSymbols);
                if (dsym < 0 || dsym >= 30) { cleanup(); return QByteArray(); }
                int dist = distBase[dsym];
                int dextra = br.getBits(distExtra[dsym]);
                if (dextra < 0) { cleanup(); return QByteArray(); }
                dist += dextra;
                if ((size_t)dist > (size_t)out.size()) { cleanup(); return QByteArray(); }
                int start = out.size() - dist;
                for (int k = 0; k < len; k++)
                    out.append(out[start + k]);
            }
        }
        cleanup();
    } while (!final);

    return out;
}

// ---------- ZIP reading ----------
bool readZip(const QString& path, QMap<QString, QByteArray>& out) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QByteArray data = f.readAll();
    f.close();

    qint64 eocd = -1;
    for (qint64 i = data.size() - 22; i >= 0; i--) {
        if ((quint8)data[i] == 0x50 && (quint8)data[i + 1] == 0x4B &&
            (quint8)data[i + 2] == 0x05 && (quint8)data[i + 3] == 0x06) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) return false;

    quint32 cdOffset = readLE32(data, eocd + 16);
    quint16 cdCount = readLE16(data, eocd + 10);
    qint64 p = cdOffset;

    for (int e = 0; e < cdCount; e++) {
        if (p + 46 > data.size()) return false;
        if (!((quint8)data[p] == 0x50 && (quint8)data[p + 1] == 0x4B &&
              (quint8)data[p + 2] == 0x01 && (quint8)data[p + 3] == 0x02))
            return false;
        quint16 method = readLE16(data, p + 10);
        quint32 compSize = readLE32(data, p + 20);
        quint16 fnLen = readLE16(data, p + 28);
        quint16 extraLen = readLE16(data, p + 30);
        quint16 commLen = readLE16(data, p + 32);
        quint32 localOffset = readLE32(data, p + 42);
        QString name = QString::fromUtf8(data.mid(p + 46, fnLen));

        if (localOffset + 30 > (quint32)data.size()) return false;
        quint16 lfnLen = readLE16(data, localOffset + 26);
        quint16 lextraLen = readLE16(data, localOffset + 28);
        qint64 dataStart = (qint64)localOffset + 30 + lfnLen + lextraLen;
        if (dataStart + compSize > (quint32)data.size()) return false;

        QByteArray comp = data.mid(dataStart, compSize);
        QByteArray content;
        if (method == 0)
            content = comp;
        else if (method == 8)
            content = inflateRaw(comp);
        else
            return false;

        out[name] = content;
        p += 46 + fnLen + extraLen + commLen;
    }
    return true;
}

// ---------- ZIP writing (STORE) ----------
bool writeZip(const QString& path, const QMap<QString, QByteArray>& files) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;

    QByteArray cd;
    quint32 offset = 0;
    for (auto it = files.begin(); it != files.end(); ++it) {
        QByteArray name = it.key().toUtf8();
        QByteArray content = it.value();
        quint32 crc = crc32(content);

        QByteArray lh;
        appendLE32(lh, 0x04034b50);
        appendLE16(lh, 20);
        appendLE16(lh, 0);
        appendLE16(lh, 0); // method 0 = store
        appendLE16(lh, 0);
        appendLE16(lh, 0);
        appendLE32(lh, crc);
        appendLE32(lh, (quint32)content.size());
        appendLE32(lh, (quint32)content.size());
        appendLE16(lh, (quint16)name.size());
        appendLE16(lh, 0);
        f.write(lh);
        f.write(name);
        f.write(content);

        QByteArray ce;
        appendLE32(ce, 0x02014b50);
        appendLE16(ce, 20);
        appendLE16(ce, 20);
        appendLE16(ce, 0);
        appendLE16(ce, 0);
        appendLE16(ce, 0);
        appendLE16(ce, 0);
        appendLE32(ce, crc);
        appendLE32(ce, (quint32)content.size());
        appendLE32(ce, (quint32)content.size());
        appendLE16(ce, (quint16)name.size());
        appendLE16(ce, 0);
        appendLE16(ce, 0);
        appendLE16(ce, 0);
        appendLE16(ce, 0);
        appendLE32(ce, 0);
        appendLE32(ce, offset);
        ce.append(name);
        cd.append(ce);

        offset += (quint32)lh.size() + (quint32)name.size() + (quint32)content.size();
    }

    quint32 cdOffset = offset;
    quint32 cdSize = (quint32)cd.size();
    f.write(cd);

    QByteArray eo;
    appendLE32(eo, 0x06054b50);
    appendLE16(eo, 0);
    appendLE16(eo, 0);
    appendLE16(eo, (quint16)files.size());
    appendLE16(eo, (quint16)files.size());
    appendLE32(eo, cdSize);
    appendLE32(eo, cdOffset);
    appendLE16(eo, 0);
    f.write(eo);

    f.close();
    return true;
}

// ---------- self test ----------
bool selfTest() {
    QList<QByteArray> samples;
    samples.append(QByteArray("Hello, this is a test of the DEFLATE inflater used by the "
                              "OpenOffice docx engine. It must correctly reconstruct arbitrary "
                              "text and binary data without corruption."));
    samples.append(QByteArray("AAAAAAABBBBCCCCDDDDEEEEAAAAAAABBBBCCCCDDDDEEEE"));
    samples.append(QByteArray("The quick brown fox jumps over the lazy dog. "
                              "Pack my box with five dozen liquor jugs. "
                              "How vexingly quick daft zebras jump!"));

    for (const QByteArray& s : samples) {
        QByteArray z = qCompress(s, 9);
        // qCompress: [4-byte size][zlib stream]. zlib = [2 hdr][deflate][4 adler]
        QByteArray raw = z.mid(6, z.size() - 10);
        QByteArray out = inflateRaw(raw);
        if (out != s) return false;
    }
    return true;
}

} // namespace ZipUtil
