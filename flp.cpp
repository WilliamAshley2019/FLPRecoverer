#include "flp.h"
#include <cmath>
#include <cstring>

namespace FL
{

    // =============================================================================
    // 2.  Version helper
    // =============================================================================
    bool FLVersion::operator>=(const FLVersion& other) const
    {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        if (patch != other.patch) return patch > other.patch;
        return build >= other.build;
    }

    // =============================================================================
    // 3.  Varint helpers
    // =============================================================================
    static int readVarInt(juce::InputStream& in)
    {
        int result = 0;
        int shift = 0;
        uint8_t byte;
        do {
            if (in.read(&byte, 1) != 1) throw std::runtime_error("Unexpected EOF");
            result |= (int)(byte & 0x7F) << shift;
            shift += 7;
        } while (byte & 0x80);
        return result;
    }

    static void writeVarInt(juce::OutputStream& out, int value)
    {
        uint8_t byte = value & 0x7F;
        while (value > 0x7F) {
            out.writeByte(byte | 0x80);
            value >>= 7;
            byte = value & 0x7F;
        }
        out.writeByte(byte);
    }

    // =============================================================================
    // 3b. Small binary helpers
    //     Real JUCE (unlike what earlier drafts of this file assumed) has no
    //     littleEndianUint16/32, littleEndianFloat/Double, floatToRawLEBytes,
    //     InputStream::skip/skipBytes, MemoryOutputStream::writeUTF16, or
    //     String::fromUTF16. These wrap what JUCE actually provides:
    //     ByteOrder::littleEndianShort/littleEndianInt for integers, and
    //     manual bit-reinterpretation for floats/doubles. JUCE's own
    //     InputStream::read()/OutputStream::write() are plain byte copies,
    //     so decoding "as little-endian" after a raw read is correct on every
    //     platform (identity on LE hosts, byte-swapped on BE hosts).
    // =============================================================================
    static uint16_t leU16FromBytes(const void* bytes)
    {
        return (uint16_t) juce::ByteOrder::littleEndianShort(bytes);
    }

    static uint32_t leU32FromBytes(const void* bytes)
    {
        return juce::ByteOrder::littleEndianInt(bytes);
    }

    static float leFloatFromBytes(const void* bytes)
    {
        uint32_t bits = juce::ByteOrder::littleEndianInt(bytes);
        float f;
        std::memcpy(&f, &bits, sizeof(float));
        return f;
    }

    static double leDoubleFromBytes(const void* bytes)
    {
        uint64_t bits = juce::ByteOrder::littleEndianInt64(bytes);
        double d;
        std::memcpy(&d, &bits, sizeof(double));
        return d;
    }

    static void writeFloatLE(juce::OutputStream& out, float value)
    {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        uint32_t le = juce::ByteOrder::swapIfBigEndian(bits);
        out.write(&le, 4);
    }

    static void writeDoubleLE(juce::OutputStream& out, double value)
    {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        uint64_t le = juce::ByteOrder::swapIfBigEndian(bits);
        out.write(&le, 8);
    }

    static void writeZeros(juce::OutputStream& out, int numBytes)
    {
        for (int i = 0; i < numBytes; ++i) out.writeByte(0);
    }

    // Decode a little-endian UTF-16 byte buffer (as stored in FLP unicode text
    // events) into a juce::String, stopping at a null terminator if present.
    static juce::String decodeUTF16LE(const uint8_t* bytes, size_t byteLen)
    {
        juce::String result;
        size_t numUnits = byteLen / 2;
        for (size_t i = 0; i < numUnits; ++i)
        {
            uint16_t unit = leU16FromBytes(bytes + i * 2);
            if (unit == 0) break; // null terminator

            if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < numUnits)
            {
                uint16_t low = leU16FromBytes(bytes + (i + 1) * 2);
                if (low >= 0xDC00 && low <= 0xDFFF)
                {
                    uint32_t cp = 0x10000 + ((uint32_t)(unit - 0xD800) << 10) + (low - 0xDC00);
                    result += juce::String::charToString((juce::juce_wchar) cp);
                    ++i;
                    continue;
                }
            }
            result += juce::String::charToString((juce::juce_wchar) unit);
        }
        return result;
    }

    // Strip a single trailing NUL character that FLP ASCII text events are
    // typically terminated with (fromUTF8 with an explicit length can leave
    // an embedded trailing '\0' character in the resulting String).
    static juce::String stripTrailingNul(juce::String s)
    {
        while (s.isNotEmpty() && s[s.length() - 1] == 0)
            s = s.dropLastCharacters(1);
        return s;
    }


    // =============================================================================
    // 4.  Event implementations
    // =============================================================================

    void U8Event::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        out.writeByte(value);
    }

    void BoolEvent::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        out.writeByte(value ? 1 : 0);
    }

    void I16Event::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        uint16_t le = juce::ByteOrder::swapIfBigEndian((uint16_t)value);
        out.write(&le, 2);
    }

    void U16Event::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        uint16_t le = juce::ByteOrder::swapIfBigEndian(value);
        out.write(&le, 2);
    }

    void I32Event::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        uint32_t le = juce::ByteOrder::swapIfBigEndian((uint32_t)value);
        out.write(&le, 4);
    }

    void U32Event::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        uint32_t le = juce::ByteOrder::swapIfBigEndian(value);
        out.write(&le, 4);
    }

    void F32Event::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        writeFloatLE(out, value);
    }

    void ColorEvent::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        // Colour::getRed()/getGreen()/getBlue()/getAlpha() already return 0-255 uint8s.
        uint32_t rgba = (uint32_t)(value.getRed())   << 0  |
                        (uint32_t)(value.getGreen()) << 8  |
                        (uint32_t)(value.getBlue())  << 16 |
                        (uint32_t)(value.getAlpha()) << 24;
        uint32_t le = juce::ByteOrder::swapIfBigEndian(rgba);
        out.write(&le, 4);
    }

    void AsciiEvent::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        auto utf8 = value.toRawUTF8();
        int len = (int) strlen(utf8) + 1; // include trailing NUL terminator
        writeVarInt(out, len);
        out.write(utf8, (size_t) len);
    }

    void UnicodeEvent::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        juce::MemoryOutputStream mos;
        for (auto charPtr = value.getCharPointer(); !charPtr.isEmpty(); )
        {
            juce::juce_wchar cp = charPtr.getAndAdvance();
            if (cp <= 0xFFFF)
            {
                mos.writeShort((short) (uint16_t) cp);
            }
            else
            {
                uint32_t v = (uint32_t) cp - 0x10000;
                uint16_t hi = (uint16_t) (0xD800 + (v >> 10));
                uint16_t lo = (uint16_t) (0xDC00 + (v & 0x3FF));
                mos.writeShort((short) hi);
                mos.writeShort((short) lo);
            }
        }
        mos.writeShort(0); // NUL terminator
        juce::MemoryBlock mb = mos.getMemoryBlock();
        writeVarInt(out, (int) mb.getSize());
        out.write(mb.getData(), mb.getSize());
    }

    void StructEvent::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        juce::MemoryOutputStream mos;
        writeFields(mos);
        juce::MemoryBlock mb = mos.getMemoryBlock();
        if (static_cast<uint8_t>(m_id) >= 192) {
            writeVarInt(out, (int)mb.getSize());
        }
        out.write(mb.getData(), mb.getSize());
    }

    void ListEvent::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        juce::MemoryOutputStream mos;
        writeItems(mos);
        juce::MemoryBlock mb = mos.getMemoryBlock();
        if (static_cast<uint8_t>(m_id) >= 192) {
            writeVarInt(out, (int)mb.getSize());
        }
        out.write(mb.getData(), mb.getSize());
    }

    void UnknownDataEvent::write(juce::OutputStream& out) const
    {
        out.writeByte(static_cast<uint8_t>(m_id));
        if (static_cast<uint8_t>(m_id) >= 192) {
            writeVarInt(out, (int)m_data.getSize());
        }
        out.write(m_data.getData(), m_data.getSize());
    }

    // =============================================================================
    // 5.  Event factory – using juce::ByteOrder for endian conversion
    // =============================================================================
    std::unique_ptr<Event> Event::read(juce::InputStream& in, EventID id, const FLVersion& version)
    {
        uint8_t idVal = static_cast<uint8_t>(id);
        size_t dataSize;
        if (idVal < 64) dataSize = 1;
        else if (idVal < 128) dataSize = 2;
        else if (idVal < 192) {
            // EventID 172 is a confirmed exception to the standard 4-byte
            // dword-range rule -   Reproduced identically across
            // two independent real FL 26 files; not yet cross-checked
            // against any external documentation.
            dataSize = (idVal == 172) ? 3 : 4;
        }
        else {
            dataSize = readVarInt(in);
        }

        juce::HeapBlock<uint8_t> buffer(dataSize);
        if (in.read(buffer.get(), dataSize) != (int)dataSize)
            throw std::runtime_error("Failed to read event data");

        juce::MemoryInputStream memIn(buffer.get(), dataSize, false);

        switch (id) {
            // ----- Fixed-size events -----
        case EventID::IsEnabled:      return std::make_unique<BoolEvent>(id, buffer[0] != 0);
        case EventID::VolByte:        return std::make_unique<U8Event>(id, buffer[0]);
        case EventID::PanByte:        return std::make_unique<U8Event>(id, buffer[0]);
        case EventID::Zipped:         return std::make_unique<BoolEvent>(id, buffer[0] != 0);
        case EventID::LoopActive:     return std::make_unique<BoolEvent>(id, buffer[0] != 0);
        case EventID::Shuffle:        return std::make_unique<BoolEvent>(id, buffer[0] != 0);
        case EventID::ShowInfo:       return std::make_unique<BoolEvent>(id, buffer[0] != 0);
        case EventID::MainVol:        return std::make_unique<U8Event>(id, buffer[0]);
        case EventID::ChanType:       return std::make_unique<U8Event>(id, buffer[0]);
        case EventID::PanLaw:         return std::make_unique<U8Event>(id, buffer[0]);
        case EventID::IsLocked:       return std::make_unique<BoolEvent>(id, buffer[0] != 0);
        case EventID::TimeSigNumerator: return std::make_unique<U8Event>(id, buffer[0]);
        case EventID::TimeSigDenominator: return std::make_unique<U8Event>(id, buffer[0]);
        case EventID::Licensed:       return std::make_unique<BoolEvent>(id, buffer[0] != 0);
        case EventID::PlayTruncatedNotes: return std::make_unique<BoolEvent>(id, buffer[0] != 0);

        case EventID::NewChan: {
            uint16_t val = leU16FromBytes(buffer.get());
            return std::make_unique<U16Event>(id, val);
        }
        case EventID::NewPat: {
            uint16_t val = leU16FromBytes(buffer.get());
            return std::make_unique<U16Event>(id, val);
        }
        case EventID::TempoCoarse: {
            uint16_t val = leU16FromBytes(buffer.get());
            return std::make_unique<U16Event>(id, val);
        }
        case EventID::CurrentPatNum: {
            uint16_t val = leU16FromBytes(buffer.get());
            return std::make_unique<U16Event>(id, val);
        }
        case EventID::MainPitch: {
            int16_t val = (int16_t)leU16FromBytes(buffer.get());
            return std::make_unique<I16Event>(id, val);
        }
        case EventID::Resonance: {
            uint16_t val = leU16FromBytes(buffer.get());
            return std::make_unique<U16Event>(id, val);
        }
        case EventID::StereoDelay: {
            uint16_t val = leU16FromBytes(buffer.get());
            return std::make_unique<U16Event>(id, val);
        }
        case EventID::Swing: {
            uint16_t val = leU16FromBytes(buffer.get());
            return std::make_unique<U16Event>(id, val);
        }
        case EventID::Children: {
            uint16_t val = leU16FromBytes(buffer.get());
            return std::make_unique<U16Event>(id, val);
        }
        case EventID::InsertIcon: {
            int16_t val = (int16_t)leU16FromBytes(buffer.get());
            return std::make_unique<I16Event>(id, val);
        }
        case EventID::SlotIID: {
            int16_t val = (int16_t)leU16FromBytes(buffer.get());
            return std::make_unique<I16Event>(id, val);
        }
        case EventID::ArrangementNew: {
            uint16_t val = leU16FromBytes(buffer.get());
            return std::make_unique<U16Event>(id, val);
        }
        case EventID::CurrentlySelected: {
            uint16_t val = leU16FromBytes(buffer.get());
            return std::make_unique<U16Event>(id, val);
        }

        case EventID::Color:
        case EventID::PatternColor:
        case EventID::InsertColor:
            return std::make_unique<ColorEvent>(id, juce::Colour::fromRGBA(buffer[0], buffer[1], buffer[2], buffer[3]));

        case EventID::PluginIcon: {
            uint32_t val = leU32FromBytes(buffer.get());
            return std::make_unique<U32Event>(id, val);
        }
        case EventID::Tempo: {
            uint32_t val = leU32FromBytes(buffer.get());
            return std::make_unique<U32Event>(id, val);
        }
        case EventID::MarkerPosition: {
            uint32_t val = leU32FromBytes(buffer.get());
            return std::make_unique<U32Event>(id, val);
        }
        case EventID::WindowHeight: {
            uint32_t val = leU32FromBytes(buffer.get());
            return std::make_unique<U32Event>(id, val);
        }
        case EventID::GroupNum: {
            int32_t val = (int32_t)leU32FromBytes(buffer.get());
            return std::make_unique<I32Event>(id, val);
        }
        case EventID::SamplerFlags: {
            uint32_t val = leU32FromBytes(buffer.get());
            return std::make_unique<U32Event>(id, val);
        }
        case EventID::LayerFlags: {
            uint32_t val = leU32FromBytes(buffer.get());
            return std::make_unique<U32Event>(id, val);
        }
        case EventID::PatternSteps: {
            uint32_t val = leU32FromBytes(buffer.get());
            return std::make_unique<U32Event>(id, val);
        }
        case EventID::VerBuild: {
            uint32_t val = leU32FromBytes(buffer.get());
            return std::make_unique<U32Event>(id, val);
        }
        case EventID::VersionBuild: {
            uint32_t val = leU32FromBytes(buffer.get());
            return std::make_unique<U32Event>(id, val);
        }
        case EventID::IntStretch: {
            float val = leFloatFromBytes(buffer.get());
            return std::make_unique<F32Event>(id, val);
        }

        case EventID::MarkerText:
        case EventID::PatName:
        case EventID::Title:
        case EventID::Comment:
        case EventID::URL:
        case EventID::CommentRTF:
        case EventID::Licensee:
        case EventID::DataPath:
        case EventID::Genre:
        case EventID::Author:
        case EventID::TrackName:
        case EventID::ArrangementName:
        case EventID::CategoryName:
        case EventID::PluginFactory:
        case EventID::PluginName:
        case EventID::SampleFileName:
        case EventID::Version:
            if (version >= FLVersion{ 11,5,0,0 })
                return std::make_unique<UnicodeEvent>(id, decodeUTF16LE(buffer.get(), dataSize));
            else
                return std::make_unique<AsciiEvent>(id, stripTrailingNul(juce::String::fromUTF8(reinterpret_cast<const char*>(buffer.get()), (int)dataSize)));

            // ----- Structured events -----
        case EventID::TrackInfo: {
            auto ev = std::make_unique<TrackInfoEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::Playlist: {
            auto ev = std::make_unique<PlaylistEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::Levels: {
            auto ev = std::make_unique<LevelsEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::ChanParams: {
            auto ev = std::make_unique<ChannelBlobEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::RemoteController: {
            auto ev = std::make_unique<RemoteControllerEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::Automation: {
            auto ev = std::make_unique<AutomationEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::MixerBlob: {
            auto ev = std::make_unique<MixerBlobEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::NewPlugin: {
            auto ev = std::make_unique<WrapperEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::PatternNotes: {
            auto ev = std::make_unique<PatternNotesEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::PatternCtrls: {
            auto ev = std::make_unique<PatternCtrlsEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::InsertData: {
            auto ev = std::make_unique<InsertDataEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::InsertRouting: {
            auto ev = std::make_unique<InsertRoutingEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        case EventID::Timestamp: {
            auto ev = std::make_unique<TimestampEvent>();
            ev->parse(memIn, dataSize);
            return ev;
        }
        default:
            return std::make_unique<UnknownDataEvent>(id, buffer.get(), dataSize);
        }
    }

    // =============================================================================
    // 6.  Specific event implementations – using juce::ByteOrder
    // =============================================================================

    void TrackInfoEvent::parse(juce::InputStream& in, size_t size)
    {
        auto& f = fields;
        if (size < 4) return;
        uint32_t v; in.read(&v, 4); f.iid = leU32FromBytes(&v);
        if (size < 8) return;
        uint8_t r, g, b, a; in.read(&r, 1); in.read(&g, 1); in.read(&b, 1); in.read(&a, 1);
        f.color = juce::Colour(r, g, b);
        if (size < 12) return;
        in.read(&v, 4); f.icon = leU32FromBytes(&v);
        if (size < 13) return;
        uint8_t en; in.read(&en, 1); f.enabled = (en != 0);
        if (size < 17) return;
        float fl; in.read(&fl, 4); f.height = leFloatFromBytes(&fl);
        if (size < 21) return;
        int32_t lh; in.read(&lh, 4); f.lockedHeight = (int32_t)leU32FromBytes(&lh);
        if (size < 22) return;
        uint8_t cl; in.read(&cl, 1); f.contentLocked = (cl != 0);
        if (size < 26) return;
        in.read(&v, 4); f.motion = leU32FromBytes(&v);
        if (size < 30) return;
        in.read(&v, 4); f.press = leU32FromBytes(&v);
        if (size < 34) return;
        in.read(&v, 4); f.triggerSync = leU32FromBytes(&v);
        if (size < 38) return;
        uint32_t q; in.read(&q, 4); f.queued = (q != 0);
        if (size < 42) return;
        uint32_t tol; in.read(&tol, 4); f.tolerant = (tol != 0);
        if (size < 46) return;
        in.read(&v, 4); f.positionSync = leU32FromBytes(&v);
        if (size < 47) return;
        uint8_t gr; in.read(&gr, 1); f.grouped = (gr != 0);
        if (size < 48) return;
        uint8_t lk; in.read(&lk, 1); f.locked = (lk != 0);
    }

    void TrackInfoEvent::writeFields(juce::OutputStream& out) const
    {
        auto writeU32 = [&](uint32_t val) { uint32_t le = juce::ByteOrder::swapIfBigEndian(val); out.write(&le, 4); };
        auto writeI32 = [&](int32_t val) { uint32_t le = juce::ByteOrder::swapIfBigEndian((uint32_t)val); out.write(&le, 4); };
        auto writeFloat = [&](float val) { writeFloatLE(out, val); };
        auto writeBool = [&](bool val) { out.writeByte(val ? 1 : 0); };

        writeU32(fields.iid);
        out.writeByte(fields.color.getRed());
        out.writeByte(fields.color.getGreen());
        out.writeByte(fields.color.getBlue());
        out.writeByte(fields.color.getAlpha());
        writeU32(fields.icon);
        writeBool(fields.enabled);
        writeFloat(fields.height);
        writeI32(fields.lockedHeight);
        writeBool(fields.contentLocked);
        writeU32(fields.motion);
        writeU32(fields.press);
        writeU32(fields.triggerSync);
        writeU32(fields.queued ? 1 : 0);
        writeU32(fields.tolerant ? 1 : 0);
        writeU32(fields.positionSync);
        writeBool(fields.grouped);
        writeBool(fields.locked);
    }

    int PlaylistEvent::detectItemSize(const uint8_t* data, size_t totalSize, int offset)
    {
        // 88 confirmed against two real FL 26.1.0.5530 project files - without
        // it, detectItemSize returns 0 for those files and PlaylistEvent::parse
        // silently produces zero items (see the itemSize==0 early-return
        // below), so any project made in that FL build reports an empty
        // arrangement/playlist even though PlaylistItem data is present and
        // decodes cleanly at itemSize=88.
        const int candidates[] = { 32, 60, 64, 80, 88, 320 };
        for (int cand : candidates) {
            if (totalSize % cand != 0) continue;
            int n = totalSize / cand;
            bool ok = true;
            for (int i = 0; i < n; ++i) {
                uint16_t pb = leU16FromBytes(data + offset + i * cand + 4);
                if (pb != 20480) { ok = false; break; }
            }
            if (ok) return cand;
        }
        int best = 0;
        float bestRatio = 0.5f;
        for (int cand : candidates) {
            if (totalSize % cand != 0) continue;
            int n = totalSize / cand;
            int good = 0;
            for (int i = 0; i < n; ++i) {
                uint16_t pb = leU16FromBytes(data + offset + i * cand + 4);
                if (pb == 20480) ++good;
            }
            float ratio = (float)good / n;
            if (ratio > bestRatio) { bestRatio = ratio; best = cand; }
        }
        return best;
    }

    void PlaylistEvent::parse(juce::InputStream& in, size_t size)
    {
        juce::HeapBlock<uint8_t> buf(size);
        if (in.read(buf.get(), size) != (int)size)
            throw std::runtime_error("Failed to read Playlist data");
        int itemSize = detectItemSize(buf.get(), size, 0);
        if (itemSize == 0) { m_itemSize = 80; return; }
        m_itemSize = itemSize;
        int count = size / itemSize;
        items.clear();
        items.reserve(count);
        for (int i = 0; i < count; ++i) {
            int base = i * itemSize;
            PlaylistItem it;
            it.position = leU32FromBytes(buf.get() + base);
            it.patternBase = leU16FromBytes(buf.get() + base + 4);
            it.itemIndex = leU16FromBytes(buf.get() + base + 6);
            it.length = leU32FromBytes(buf.get() + base + 8);
            it.trackRvidx = leU16FromBytes(buf.get() + base + 12);
            it.group = leU16FromBytes(buf.get() + base + 14);
            it.itemFlags = leU16FromBytes(buf.get() + base + 18);
            it.startOffset = leFloatFromBytes(buf.get() + base + 24);
            it.endOffset = leFloatFromBytes(buf.get() + base + 28);
            items.push_back(it);
        }
    }

    void PlaylistEvent::writeItems(juce::OutputStream& out) const
    {
        for (const auto& it : items) {
            juce::MemoryOutputStream mos;
            uint32_t val = juce::ByteOrder::swapIfBigEndian(it.position); mos.write(&val, 4);
            uint16_t val16 = juce::ByteOrder::swapIfBigEndian(it.patternBase); mos.write(&val16, 2);
            val16 = juce::ByteOrder::swapIfBigEndian(it.itemIndex); mos.write(&val16, 2);
            val = juce::ByteOrder::swapIfBigEndian(it.length); mos.write(&val, 4);
            val16 = juce::ByteOrder::swapIfBigEndian(it.trackRvidx); mos.write(&val16, 2);
            val16 = juce::ByteOrder::swapIfBigEndian(it.group); mos.write(&val16, 2);
            val16 = 0x0078; mos.write(&val16, 2);
            val16 = juce::ByteOrder::swapIfBigEndian(it.itemFlags); mos.write(&val16, 2);
            uint32_t magic = 0x80806440; val = juce::ByteOrder::swapIfBigEndian(magic); mos.write(&val, 4);
            float f = juce::ByteOrder::swapIfBigEndian(it.startOffset); mos.write(&f, 4);
            f = juce::ByteOrder::swapIfBigEndian(it.endOffset); mos.write(&f, 4);
            size_t written = mos.getDataSize();
            if (written < (size_t)m_itemSize) {
                int padding = m_itemSize - (int)written;
                for (int i = 0; i < padding; ++i) mos.writeByte(0);
            }
            out.write(mos.getData(), mos.getDataSize());
        }
    }

    void LevelsEvent::parse(juce::InputStream& in, size_t size)
    {
        auto& f = fields;
        if (size >= 4) { uint32_t v; in.read(&v, 4); f.pan = leU32FromBytes(&v); }
        if (size >= 8) { uint32_t v; in.read(&v, 4); f.volume = leU32FromBytes(&v); }
        if (size >= 12) { int32_t v; in.read(&v, 4); f.pitchShift = (int32_t)leU32FromBytes(&v); }
        if (size >= 16) { uint32_t v; in.read(&v, 4); f.filterModX = leU32FromBytes(&v); }
        if (size >= 20) { uint32_t v; in.read(&v, 4); f.filterModY = leU32FromBytes(&v); }
        if (size >= 24) { uint32_t v; in.read(&v, 4); f.filterType = leU32FromBytes(&v); }
    }

    void LevelsEvent::writeFields(juce::OutputStream& out) const
    {
        auto writeOptU32 = [&](const std::optional<uint32_t>& opt) {
            if (opt) {
                uint32_t le = juce::ByteOrder::swapIfBigEndian(*opt);
                out.write(&le, 4);
            }
            else {
                uint32_t zero = 0; out.write(&zero, 4);
            }
            };
        auto writeOptI32 = [&](const std::optional<int32_t>& opt) {
            if (opt) {
                uint32_t le = juce::ByteOrder::swapIfBigEndian((uint32_t)*opt);
                out.write(&le, 4);
            }
            else {
                uint32_t zero = 0; out.write(&zero, 4);
            }
            };
        writeOptU32(fields.pan);
        writeOptU32(fields.volume);
        writeOptI32(fields.pitchShift);
        writeOptU32(fields.filterModX);
        writeOptU32(fields.filterModY);
        writeOptU32(fields.filterType);
    }

    void ChannelBlobEvent::parse(juce::InputStream& in, size_t size)
    {
        rawData.setSize(size);
        if (size > 0) {
            if (in.read(rawData.getData(), size) != (int)size)
                throw std::runtime_error("Failed to read ChannelBlob data");
        }
    }
    void ChannelBlobEvent::writeFields(juce::OutputStream& out) const
    {
        out.write(rawData.getData(), rawData.getSize());
    }

    bool ChannelBlobEvent::getNoDC() const {
        if (rawData.getSize() < 10) return false;
        return rawData[9] != 0;
    }
    void ChannelBlobEvent::setNoDC(bool val) {
        if (rawData.getSize() < 10) return;
        rawData[9] = val ? 1 : 0;
    }
    uint8_t ChannelBlobEvent::getDelayFlags() const {
        if (rawData.getSize() < 11) return 0;
        return rawData[10];
    }
    void ChannelBlobEvent::setDelayFlags(uint8_t flags) {
        if (rawData.getSize() < 11) return;
        rawData[10] = flags;
    }
    bool ChannelBlobEvent::getUseMainPitch() const {
        if (rawData.getSize() < 12) return false;
        return rawData[11] != 0;
    }
    void ChannelBlobEvent::setUseMainPitch(bool val) {
        if (rawData.getSize() < 12) return;
        rawData[11] = val ? 1 : 0;
    }
    uint32_t ChannelBlobEvent::getArpDirection() const {
        if (rawData.getSize() < 44) return 0;
        return leU32FromBytes((uint8_t*)rawData.getData() + 40);
    }
    void ChannelBlobEvent::setArpDirection(uint32_t dir) {
        if (rawData.getSize() < 44) return;
        uint32_t le = juce::ByteOrder::swapIfBigEndian(dir);
        memcpy((uint8_t*)rawData.getData() + 40, &le, 4);
    }

    void RemoteControllerEvent::parse(juce::InputStream& in, size_t size)
    {
        if (size < 20) return; // 2+4+2+2+2+4+4 = 20 bytes total; was previously checking < 22, which discarded every real instance
        in.read(&fields.unknown1, 2);
        fields.unknown1 = leU16FromBytes(&fields.unknown1);
        in.read(&fields.trackId, 4);
        fields.trackId = leU32FromBytes(&fields.trackId);
        in.read(&fields.unknown2, 2);
        fields.unknown2 = leU16FromBytes(&fields.unknown2);
        in.read(&fields.paramId, 2);
        fields.paramId = leU16FromBytes(&fields.paramId);
        in.read(&fields.destId, 2);
        fields.destId = leU16FromBytes(&fields.destId);
        in.read(&fields.unknown3, 4);
        fields.unknown3 = leU32FromBytes(&fields.unknown3);
        in.read(&fields.unknown4, 4);
        fields.unknown4 = leU32FromBytes(&fields.unknown4);
    }
    void RemoteControllerEvent::writeFields(juce::OutputStream& out) const
    {
        auto writeU16 = [&](uint16_t val) { uint16_t le = juce::ByteOrder::swapIfBigEndian(val); out.write(&le, 2); };
        auto writeU32 = [&](uint32_t val) { uint32_t le = juce::ByteOrder::swapIfBigEndian(val); out.write(&le, 4); };
        writeU16(fields.unknown1);
        writeU32(fields.trackId);
        writeU16(fields.unknown2);
        writeU16(fields.paramId);
        writeU16(fields.destId);
        writeU32(fields.unknown3);
        writeU32(fields.unknown4);
    }

    void AutomationEvent::parse(juce::InputStream& in, size_t size)
    {
        // Reset both legacy and new storage
        points.clear();
        records.clear();
        trailingData.setSize(0);
        headerBytes.setSize(0);
        startMarkerBytes.setSize(0);

        // Read the 13-byte header verbatim - see the comment on
        // AutomationEvent::headerBytes for why we don't discard this.
        uint8_t header[HEADER_SIZE];
        if (in.read(header, HEADER_SIZE) != HEADER_SIZE)
            throw std::runtime_error("Failed to read automation header");
        headerBytes.append(header, HEADER_SIZE);

        size_t remaining = size - HEADER_SIZE;

        juce::MemoryBlock region;
        if (remaining > 0)
        {
            region.setSize(remaining);
            if (in.read(region.getData(), (int)remaining) != (int)remaining)
                throw std::runtime_error("Failed to read automation record region");
        }
        const uint8_t* data = (const uint8_t*)region.getData();
        size_t numSlots = remaining / POINT_SIZE;

        // PASS 1 - read position/value from every slot until one comes back
        // non-finite. That slot's index P is both the real point count
        // (slots 0..P-1 hold points 1..P) AND the index of the extra
        // "overflow" slot that holds the LAST point's tension/curveType
        // (see the long comment on Record - each slot's tension/curveType
        // bytes belong to the point one slot earlier than its own
        // position/value bytes, so the final point always needs one more
        // slot than its position/value alone would suggest).
        std::vector<double> slotPos, slotVal;
        size_t P = 0;
        for (; P < numSlots; ++P)
        {
            const uint8_t* raw = data + P * POINT_SIZE;
            double pos = leDoubleFromBytes(raw + 8);
            double val = leDoubleFromBytes(raw + 16);
            if (!std::isfinite(pos) || !std::isfinite(val))
                break;
            slotPos.push_back(pos);
            slotVal.push_back(val);
        }
        // P is now the real point count; slot P (if present) is the
        // overflow slot holding point P's tension/curveType.

        if (P == 0)
        {
            // Nothing decodable - preserve everything verbatim rather than
            // guess, so a round-trip at least doesn't corrupt the file.
            if (remaining > 0)
                trailingData.append(data, remaining);
            return;
        }

        // Slot 0's bytes 0-7 are the opaque start marker, not tension data
        // for a real point - preserve verbatim (see startMarkerBytes).
        startMarkerBytes.append(data, 8);

        records.resize(P);
        for (size_t k = 0; k < P; ++k)
        {
            records[k].position = slotPos[k];
            records[k].value = slotVal[k];
            records[k].controlCode = (k == 0) ? leU32FromBytes(data + 0) : 0; // only slot0's is meaningful; see startMarkerBytes
            records[k].tension = 0.0f;
            records[k].curveType = -1; // -1 = "no incoming curve" (only valid state for records[0])

            // Tension/curveType for point (k+1) [1-indexed] live in slot
            // (k+1), which for the LAST point is exactly the overflow slot
            // that has no valid position/value of its own.
            size_t tensionSlot = k + 1;
            if (tensionSlot < numSlots)
            {
                const uint8_t* traw = data + tensionSlot * POINT_SIZE;
                records[k].tension = leFloatFromBytes(traw + 0);
                records[k].curveType = (int)traw[4];
            }
        }
        // records[0] structurally has no incoming curve (nothing before the
        // start point) - the tension/curveType we just read into it above
        // actually belong to POINT 2 stored one slot later... wait, no:
        // per the model, records[k]'s tension comes from slot(k+1), so
        // records[0]'s tension legitimately comes from slot(1), which is
        // real (point 1 has no incoming curve, so this is always an unused
        // default in practice, but there's nothing to strip out here).

        size_t consumedSlots = P + 1; // last point's tension/curveType slot is real and consumed
        if (consumedSlots > numSlots) consumedSlots = numSlots;
        size_t consumedBytes = consumedSlots * POINT_SIZE;
        if (consumedBytes < remaining)
            trailingData.append(data + consumedBytes, remaining - consumedBytes);

        // Also populate legacy points for backward compatibility
        for (const auto& rec : records) {
            AutomationPoint p;
            p.beatIncrement = rec.position;
            p.value = rec.value;
            p.tension = rec.tension;
            p.unknown3[0] = 0;
            p.unknown3[1] = 0;
            p.unknown3[2] = 0;
            p.direction = 0;
            points.push_back(p);
        }
    }
    void AutomationEvent::writeItems(juce::OutputStream& out) const
    {
        // Write the real on-disk header verbatim if we have one (i.e. this
        // event came from parse()); otherwise fall back to the shape of a
        // real captured header for a freshly-constructed event (e.g. from
        // AutomationEditor::generateFadeCurve, which never goes through
        // parse()).
        if (headerBytes.getSize() == HEADER_SIZE)
        {
            out.write(headerBytes.getData(), (int)HEADER_SIZE);
        }
        else
        {
            static const uint8_t kDefaultHeader[HEADER_SIZE] =
                { 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00 };
            out.write(kDefaultHeader, HEADER_SIZE);
        }

        if (records.empty())
        {
            if (trailingData.getSize() > 0)
                out.write(trailingData.getData(), (int)trailingData.getSize());
            return;
        }

        size_t P = records.size();
        // Reconstruct P+1 slots: slot k's position/value = records[k]'s
        // (for k<P); slot k's tension/curveType = records[k-1]'s (for k>=1).
        // Slot 0's tension/curveType bytes are the preserved opaque start
        // marker instead of records[-1] (which doesn't exist).
        for (size_t slotIdx = 0; slotIdx <= P; ++slotIdx)
        {
            // bytes 0-7: tension (float) + curveType (byte) + 3 reserved,
            // OR slot 0's verbatim opaque marker.
            if (slotIdx == 0)
            {
                if (startMarkerBytes.getSize() == 8)
                {
                    out.write(startMarkerBytes.getData(), 8);
                }
                else
                {
                    // Freshly-generated curve with no parsed source: write
                    // a plausible default start marker (controlCode=3,
                    // second field 0 - meaning unconfirmed either way).
                    uint32_t cc = juce::ByteOrder::swapIfBigEndian((uint32_t)3);
                    out.write(&cc, 4);
                    uint32_t zero = 0;
                    out.write(&zero, 4);
                }
            }
            else
            {
                const auto& owner = records[slotIdx - 1];
                float tension = owner.tension;
                uint32_t tbits;
                std::memcpy(&tbits, &tension, 4);
                tbits = juce::ByteOrder::swapIfBigEndian(tbits);
                out.write(&tbits, 4);
                uint8_t curveByte = (owner.curveType >= 0 && owner.curveType <= 255) ? (uint8_t)owner.curveType : 0;
                uint8_t rest[4] = { curveByte, 0, 0, 0 };
                out.write(rest, 4);
            }

            // bytes 8-23: position/value for point (slotIdx+1), i.e.
            // records[slotIdx] - only present for slotIdx < P; the final
            // (overflow) slot has no position/value of its own.
            if (slotIdx < P)
            {
                writeDoubleLE(out, records[slotIdx].position);
                writeDoubleLE(out, records[slotIdx].value);
            }
            else
            {
                // Overflow slot: position/value are meaningless (this is
                // what shows up as NaN under a naive fixed-stride read).
                // Write zeros rather than NaN so the file stays well-formed
                // even though FL itself never reads these bytes.
                writeDoubleLE(out, 0.0);
                writeDoubleLE(out, 0.0);
            }
        }

        // Write back whatever followed the real records verbatim.
        if (trailingData.getSize() > 0)
            out.write(trailingData.getData(), (int)trailingData.getSize());
    }

    void MixerBlobEvent::parse(juce::InputStream& in, size_t size)
    {
        params.clear();
        while (size >= 12) {
            MixerParam p;
            uint32_t u4; in.read(&u4, 4);
            in.read(&p.id, 1);
            in.read(&p._u1, 1);
            in.read(&p.channelData, 2);
            p.channelData = leU16FromBytes(&p.channelData);
            in.read(&p.msg, 4);
            p.msg = (int32_t)leU32FromBytes(&p.msg);
            params.push_back(p);
            size -= 12;
        }
    }
    void MixerBlobEvent::writeItems(juce::OutputStream& out) const
    {
        for (const auto& p : params) {
            uint32_t u4 = 0; out.write(&u4, 4);
            out.writeByte(p.id);
            out.writeByte(p._u1);
            uint16_t cd = juce::ByteOrder::swapIfBigEndian(p.channelData);
            out.write(&cd, 2);
            uint32_t msg = juce::ByteOrder::swapIfBigEndian((uint32_t)p.msg);
            out.write(&msg, 4);
        }
    }

    void WrapperEvent::parse(juce::InputStream& in, size_t size)
    {
        if (size < 21) return;
        in.skipNextBytes(16);
        in.read(&fields.flags, 2);
        fields.flags = leU16FromBytes(&fields.flags);
        in.skipNextBytes(2);
        in.read(&fields.page, 1);
        in.skipNextBytes(23);
        if (size >= 21 + 4 + 4) {
            in.read(&fields.width, 4);
            fields.width = leU32FromBytes(&fields.width);
            in.read(&fields.height, 4);
            fields.height = leU32FromBytes(&fields.height);
        }
    }
    void WrapperEvent::writeFields(juce::OutputStream& out) const
    {
        for (int i = 0; i < 16; ++i) out.writeByte(0);
        uint16_t fl = juce::ByteOrder::swapIfBigEndian(fields.flags);
        out.write(&fl, 2);
        uint16_t zero16 = 0; out.write(&zero16, 2);
        out.writeByte(fields.page);
        for (int i = 0; i < 23; ++i) out.writeByte(0);
        uint32_t w = juce::ByteOrder::swapIfBigEndian(fields.width);
        out.write(&w, 4);
        uint32_t h = juce::ByteOrder::swapIfBigEndian(fields.height);
        out.write(&h, 4);
    }

    void PatternNotesEvent::parse(juce::InputStream& in, size_t size)
    {
        notes.clear();
        while (size >= 24) {
            Note n;
            in.read(&n.position, 4); n.position = leU32FromBytes(&n.position);
            in.read(&n.flags, 2); n.flags = leU16FromBytes(&n.flags);
            in.read(&n.channelIID, 2); n.channelIID = leU16FromBytes(&n.channelIID);
            in.read(&n.length, 4); n.length = leU32FromBytes(&n.length);
            in.read(&n.key, 2); n.key = leU16FromBytes(&n.key);
            in.read(&n.group, 2); n.group = leU16FromBytes(&n.group);
            in.read(&n.finePitch, 1);
            in.skipNextBytes(1);
            in.read(&n.release, 1);
            in.read(&n.midiChannel, 1);
            in.read(&n.pan, 1);
            in.read(&n.velocity, 1);
            in.read(&n.modX, 1);
            in.read(&n.modY, 1);
            notes.push_back(n);
            size -= 24;
        }
    }
    void PatternNotesEvent::writeItems(juce::OutputStream& out) const
    {
        for (const auto& n : notes) {
            uint32_t pos = juce::ByteOrder::swapIfBigEndian(n.position); out.write(&pos, 4);
            uint16_t flags = juce::ByteOrder::swapIfBigEndian(n.flags); out.write(&flags, 2);
            uint16_t ciid = juce::ByteOrder::swapIfBigEndian(n.channelIID); out.write(&ciid, 2);
            uint32_t len = juce::ByteOrder::swapIfBigEndian(n.length); out.write(&len, 4);
            uint16_t key = juce::ByteOrder::swapIfBigEndian(n.key); out.write(&key, 2);
            uint16_t grp = juce::ByteOrder::swapIfBigEndian(n.group); out.write(&grp, 2);
            out.writeByte(n.finePitch);
            out.writeByte(0);
            out.writeByte(n.release);
            out.writeByte(n.midiChannel);
            out.writeByte(n.pan);
            out.writeByte(n.velocity);
            out.writeByte(n.modX);
            out.writeByte(n.modY);
        }
    }

    void PatternCtrlsEvent::parse(juce::InputStream& in, size_t size)
    {
        controllers.clear();
        while (size >= 4 + 1 + 1 + 4) {
            Controller c;
            in.read(&c.position, 4); c.position = leU32FromBytes(&c.position);
            in.skipNextBytes(2);
            in.read(&c.channelIID, 1);
            in.read(&c.flags, 1);
            in.read(&c.value, 4); c.value = leFloatFromBytes(&c.value);
            controllers.push_back(c);
            size -= 12;
        }
    }
    void PatternCtrlsEvent::writeItems(juce::OutputStream& out) const
    {
        for (const auto& c : controllers) {
            uint32_t pos = juce::ByteOrder::swapIfBigEndian(c.position); out.write(&pos, 4);
            uint16_t zero16 = 0; out.write(&zero16, 2);
            out.writeByte(c.channelIID);
            out.writeByte(c.flags);
            float val = juce::ByteOrder::swapIfBigEndian(c.value); out.write(&val, 4);
        }
    }

    void InsertDataEvent::parse(juce::InputStream& in, size_t size)
    {
        if (size < 8) return;
        in.skipNextBytes(4);
        in.read(&flags, 4); flags = leU32FromBytes(&flags);
    }
    void InsertDataEvent::writeFields(juce::OutputStream& out) const
    {
        uint32_t u4 = 0; out.write(&u4, 4);
        uint32_t f = juce::ByteOrder::swapIfBigEndian(flags);
        out.write(&f, 4);
    }

    void InsertRoutingEvent::parse(juce::InputStream& in, size_t size)
    {
        routes.clear();
        for (size_t i = 0; i < size; ++i) {
            uint8_t b; in.read(&b, 1); routes.push_back(b != 0);
        }
    }
    void InsertRoutingEvent::writeItems(juce::OutputStream& out) const
    {
        for (bool r : routes) out.writeByte(r ? 1 : 0);
    }

    void TimestampEvent::parse(juce::InputStream& in, size_t size)
    {
        if (size < 16) return;
        in.read(&createdOn, 8); createdOn = leDoubleFromBytes(&createdOn);
        in.read(&timeSpent, 8); timeSpent = leDoubleFromBytes(&timeSpent);
    }
    void TimestampEvent::writeFields(juce::OutputStream& out) const
    {
        double co = juce::ByteOrder::swapIfBigEndian(createdOn);
        out.write(&co, 8);
        double ts = juce::ByteOrder::swapIfBigEndian(timeSpent);
        out.write(&ts, 8);
    }

    // =============================================================================
    // 7.  EventTree methods (unchanged)
    // =============================================================================
    void EventTree::addEvent(std::unique_ptr<Event> event)
    {
        m_events.push_back(std::move(event));
    }
    void EventTree::removeEvent(EventID id, int index)
    {
        auto it = std::find_if(m_events.begin(), m_events.end(),
            [id, &index](const auto& e) { if (e->id() == id && index-- == 0) return true; return false; });
        if (it != m_events.end())
            m_events.erase(it);
    }
    std::vector<Event*> EventTree::getEvents(EventID id) const
    {
        std::vector<Event*> result;
        for (auto& e : m_events)
            if (e->id() == id) result.push_back(e.get());
        return result;
    }
    Event* EventTree::firstEvent(EventID id) const
    {
        for (auto& e : m_events)
            if (e->id() == id) return e.get();
        return nullptr;
    }
    bool EventTree::hasEvent(EventID id) const
    {
        return firstEvent(id) != nullptr;
    }
    void EventTree::writeAll(juce::OutputStream& out) const
    {
        for (auto& e : m_events)
            e->write(out);
    }

    // ---- Grouping methods ----
    std::vector<EventTree> EventTree::divide(EventID separator, const std::vector<EventID>& allowed) const
    {
        std::vector<EventTree> result;
        std::vector<std::unique_ptr<Event>> current;
        bool started = false; // true once we've seen at least one separator event
        for (auto& e : m_events) {
            if (e->id() == separator) {
                if (started && !current.empty()) {
                    EventTree subtree;
                    subtree.m_events = std::move(current);
                    result.push_back(std::move(subtree));
                    current.clear();
                }
                // Include the separator event itself - Channel::getIID()/
                // Pattern::getIID() look for NewChan/NewPat inside their own
                // subtree, so dropping it here would make every IID come back
                // as -1. This also means a channel/pattern with no other
                // properties still produces a (minimal) subtree instead of
                // silently vanishing.
                current.push_back(std::unique_ptr<Event>(e->clone()));
                started = true;
            }
            // Events before the first separator can't belong to any numbered
            // instance (there's nothing to attribute them to yet) - e.g. a
            // global "last selected plugin" cache entry sharing the same
            // NewPlugin/PluginParams IDs used by real channels. Without the
            // `started` guard here, that leaked in as a spurious extra group.
            else if (started && std::find(allowed.begin(), allowed.end(), e->id()) != allowed.end()) {
                current.push_back(std::unique_ptr<Event>(e->clone()));
            }
        }
        if (started && !current.empty()) {
            EventTree subtree;
            subtree.m_events = std::move(current);
            result.push_back(std::move(subtree));
        }
        return result;
    }

    std::vector<EventTree> EventTree::separate(EventID id) const
    {
        std::vector<EventTree> result;
        for (auto& e : m_events) {
            if (e->id() == id) {
                EventTree subtree;
                subtree.addEvent(std::unique_ptr<Event>(e->clone()));
                result.push_back(std::move(subtree));
            }
        }
        return result;
    }

    EventTree EventTree::subtree(std::function<bool(const Event*)> predicate) const
    {
        EventTree sub;
        for (auto& e : m_events) {
            if (predicate(e.get()))
                sub.addEvent(std::unique_ptr<Event>(e->clone()));
        }
        return sub;
    }

    // =============================================================================
    // 8.  High-level model implementations (unchanged)
    // =============================================================================

    // ---- Channel ----
    int Channel::getIID() const
    {
        auto ev = m_tree.firstEvent(EventID::NewChan);
        if (ev && dynamic_cast<U16Event*>(ev))
            return static_cast<U16Event*>(ev)->value;
        return -1;
    }

    juce::String Channel::getName() const
    {
        auto ev = m_tree.firstEvent(EventID::PluginName);
        if (ev) {
            if (auto* a = dynamic_cast<AsciiEvent*>(ev)) return a->value;
            if (auto* u = dynamic_cast<UnicodeEvent*>(ev)) return u->value;
        }
        // Very old FLP files (and channels that were simply never renamed)
        // have no explicit PluginName event at all - FL Studio itself falls
        // back to displaying the sample filename or generator type in that
        // case, so we do the same rather than reporting a blank name.
        auto sampleEv = m_tree.firstEvent(EventID::SampleFileName);
        if (sampleEv) {
            juce::String path;
            if (auto* a = dynamic_cast<AsciiEvent*>(sampleEv)) path = a->value;
            else if (auto* u = dynamic_cast<UnicodeEvent*>(sampleEv)) path = u->value;
            if (path.isNotEmpty())
                return juce::File(path).getFileNameWithoutExtension();
        }
        auto factoryEv = m_tree.firstEvent(EventID::PluginFactory);
        if (factoryEv) {
            if (auto* a = dynamic_cast<AsciiEvent*>(factoryEv)) return a->value;
            if (auto* u = dynamic_cast<UnicodeEvent*>(factoryEv)) return u->value;
        }
        return {};
    }
    void Channel::setName(const juce::String& name)
    {
        m_tree.removeEvent(EventID::PluginName);
        std::unique_ptr<Event> ev;
        if (m_version >= FLVersion{ 11,5,0,0 })
            ev = std::make_unique<UnicodeEvent>(EventID::PluginName, name);
        else
            ev = std::make_unique<AsciiEvent>(EventID::PluginName, name);
        m_tree.addEvent(std::move(ev));
    }

    juce::String Channel::getInternalName() const
    {
        auto ev = m_tree.firstEvent(EventID::PluginFactory);
        if (ev) {
            if (auto* a = dynamic_cast<AsciiEvent*>(ev)) return a->value;
            if (auto* u = dynamic_cast<UnicodeEvent*>(ev)) return u->value;
        }
        return {};
    }

    ChannelType Channel::getType() const
    {
        auto ev = m_tree.firstEvent(EventID::ChanType);
        if (ev && dynamic_cast<U8Event*>(ev))
            return static_cast<ChannelType>(static_cast<U8Event*>(ev)->value);
        return ChannelType::Sampler;
    }
    void Channel::setType(ChannelType type)
    {
        m_tree.removeEvent(EventID::ChanType);
        m_tree.addEvent(std::make_unique<U8Event>(EventID::ChanType, (uint8_t)type));
    }

    juce::Colour Channel::getColor() const
    {
        auto ev = m_tree.firstEvent(EventID::Color);
        if (ev && dynamic_cast<ColorEvent*>(ev))
            return static_cast<ColorEvent*>(ev)->value;
        return juce::Colours::grey;
    }
    void Channel::setColor(const juce::Colour& c)
    {
        m_tree.removeEvent(EventID::Color);
        m_tree.addEvent(std::make_unique<ColorEvent>(EventID::Color, c));
    }

    bool Channel::isEnabled() const
    {
        auto ev = m_tree.firstEvent(EventID::IsEnabled);
        if (ev && dynamic_cast<BoolEvent*>(ev))
            return static_cast<BoolEvent*>(ev)->value;
        return true;
    }
    void Channel::setEnabled(bool enabled)
    {
        m_tree.removeEvent(EventID::IsEnabled);
        m_tree.addEvent(std::make_unique<BoolEvent>(EventID::IsEnabled, enabled));
    }

    bool Channel::isZipped() const
    {
        auto ev = m_tree.firstEvent(EventID::Zipped);
        if (ev && dynamic_cast<BoolEvent*>(ev))
            return static_cast<BoolEvent*>(ev)->value;
        return false;
    }
    void Channel::setZipped(bool zipped)
    {
        m_tree.removeEvent(EventID::Zipped);
        m_tree.addEvent(std::make_unique<BoolEvent>(EventID::Zipped, zipped));
    }

    bool Channel::isLocked() const
    {
        auto ev = m_tree.firstEvent(EventID::IsLocked);
        if (ev && dynamic_cast<BoolEvent*>(ev))
            return static_cast<BoolEvent*>(ev)->value;
        return false;
    }
    void Channel::setLocked(bool locked)
    {
        m_tree.removeEvent(EventID::IsLocked);
        m_tree.addEvent(std::make_unique<BoolEvent>(EventID::IsLocked, locked));
    }

    LevelsEvent* Channel::getLevelsEvent() const
    {
        auto ev = m_tree.firstEvent(EventID::Levels);
        return dynamic_cast<LevelsEvent*>(ev);
    }
    void Channel::ensureLevelsEvent()
    {
        if (!m_tree.hasEvent(EventID::Levels))
            m_tree.addEvent(std::make_unique<LevelsEvent>());
    }

    std::optional<int> Channel::getVolume() const
    {
        auto* levels = getLevelsEvent();
        if (levels && levels->fields.volume.has_value())
            return (int)*levels->fields.volume;
        auto evWord = m_tree.firstEvent(EventID::VolByte);
        if (evWord && dynamic_cast<U8Event*>(evWord))
            return static_cast<U8Event*>(evWord)->value;
        return std::nullopt;
    }
    void Channel::setVolume(int vol)
    {
        auto* levels = getLevelsEvent();
        if (levels) {
            levels->fields.volume = (uint32_t)vol;
            return;
        }
        m_tree.removeEvent(EventID::VolByte);
        m_tree.addEvent(std::make_unique<U8Event>(EventID::VolByte, (uint8_t)vol)); // was wrongly U16Event (2 bytes) for a byte-range ID
    }

    std::optional<int> Channel::getPan() const
    {
        auto* levels = getLevelsEvent();
        if (levels && levels->fields.pan.has_value())
            return (int)*levels->fields.pan;
        auto ev = m_tree.firstEvent(EventID::PanByte);
        if (ev && dynamic_cast<U8Event*>(ev))
            return static_cast<U8Event*>(ev)->value;
        return std::nullopt;
    }
    void Channel::setPan(int pan)
    {
        auto* levels = getLevelsEvent();
        if (levels) {
            levels->fields.pan = (uint32_t)pan;
            return;
        }
        m_tree.removeEvent(EventID::PanByte);
        m_tree.addEvent(std::make_unique<U8Event>(EventID::PanByte, (uint8_t)pan)); // was wrongly U16Event (2 bytes) for a byte-range ID
    }

    std::optional<int> Channel::getPitchShift() const
    {
        auto* levels = getLevelsEvent();
        if (levels && levels->fields.pitchShift.has_value())
            return (int)*levels->fields.pitchShift;
        return std::nullopt;
    }
    void Channel::setPitchShift(int cents)
    {
        ensureLevelsEvent();
        auto* levels = getLevelsEvent();
        if (levels) levels->fields.pitchShift = cents;
    }

    std::optional<int> Channel::getFilterCutoff() const
    {
        auto* levels = getLevelsEvent();
        if (levels && levels->fields.filterModX.has_value())
            return (int)*levels->fields.filterModX;
        return std::nullopt;
    }
    void Channel::setFilterCutoff(int value)
    {
        ensureLevelsEvent();
        auto* levels = getLevelsEvent();
        if (levels) levels->fields.filterModX = (uint32_t)value;
    }

    std::optional<int> Channel::getFilterResonance() const
    {
        auto* levels = getLevelsEvent();
        if (levels && levels->fields.filterModY.has_value())
            return (int)*levels->fields.filterModY;
        return std::nullopt;
    }
    void Channel::setFilterResonance(int value)
    {
        ensureLevelsEvent();
        auto* levels = getLevelsEvent();
        if (levels) levels->fields.filterModY = (uint32_t)value;
    }

    std::optional<int> Channel::getFilterType() const
    {
        auto* levels = getLevelsEvent();
        if (levels && levels->fields.filterType.has_value())
            return (int)*levels->fields.filterType;
        return std::nullopt;
    }
    void Channel::setFilterType(int type)
    {
        ensureLevelsEvent();
        auto* levels = getLevelsEvent();
        if (levels) levels->fields.filterType = (uint32_t)type;
    }

    int Channel::getGroupIndex() const
    {
        auto ev = m_tree.firstEvent(EventID::GroupNum);
        if (ev && dynamic_cast<I32Event*>(ev))
            return static_cast<I32Event*>(ev)->value;
        return 0;
    }
    void Channel::setGroupIndex(int group)
    {
        m_tree.removeEvent(EventID::GroupNum);
        m_tree.addEvent(std::make_unique<I32Event>(EventID::GroupNum, group));
    }

    std::unique_ptr<Channel> Channel::create(EventTree& tree, const FLVersion& version)
    {
        return std::unique_ptr<Channel>(new Channel(tree, version));
    }

    // ---- Pattern ----
    int Pattern::getIID() const
    {
        auto ev = m_tree.firstEvent(EventID::NewPat);
        if (ev && dynamic_cast<U16Event*>(ev))
            return static_cast<U16Event*>(ev)->value;
        return -1;
    }
    juce::String Pattern::getName() const
    {
        auto ev = m_tree.firstEvent(EventID::PatName);
        if (ev) {
            if (auto* a = dynamic_cast<AsciiEvent*>(ev)) return a->value;
            if (auto* u = dynamic_cast<UnicodeEvent*>(ev)) return u->value;
        }
        return {};
    }
    void Pattern::setName(const juce::String& name)
    {
        m_tree.removeEvent(EventID::PatName);
        std::unique_ptr<Event> ev;
        if (m_version >= FLVersion{ 11,5,0,0 })
            ev = std::make_unique<UnicodeEvent>(EventID::PluginName, name);
        else
            ev = std::make_unique<AsciiEvent>(EventID::PluginName, name);
        m_tree.addEvent(std::move(ev));
    }
    juce::Colour Pattern::getColor() const
    {
        auto ev = m_tree.firstEvent(EventID::PatternColor);
        if (ev && dynamic_cast<ColorEvent*>(ev))
            return static_cast<ColorEvent*>(ev)->value;
        return juce::Colours::grey;
    }
    void Pattern::setColor(const juce::Colour& c)
    {
        m_tree.removeEvent(EventID::PatternColor);
        m_tree.addEvent(std::make_unique<ColorEvent>(EventID::PatternColor, c));
    }

    std::vector<Note> Pattern::getNotes() const
    {
        auto ev = m_tree.firstEvent(EventID::PatternNotes);
        if (auto* pn = dynamic_cast<PatternNotesEvent*>(ev))
            return pn->notes;
        return {};
    }
    void Pattern::setNotes(const std::vector<Note>& notes)
    {
        m_tree.removeEvent(EventID::PatternNotes);
        auto ev = std::make_unique<PatternNotesEvent>();
        ev->notes = notes;
        m_tree.addEvent(std::move(ev));
    }

    std::vector<Controller> Pattern::getControllers() const
    {
        auto ev = m_tree.firstEvent(EventID::PatternCtrls);
        if (auto* pc = dynamic_cast<PatternCtrlsEvent*>(ev))
            return pc->controllers;
        return {};
    }
    void Pattern::setControllers(const std::vector<Controller>& ctrls)
    {
        m_tree.removeEvent(EventID::PatternCtrls);
        auto ev = std::make_unique<PatternCtrlsEvent>();
        ev->controllers = ctrls;
        m_tree.addEvent(std::move(ev));
    }

    int Pattern::getLength() const
    {
        auto ev = m_tree.firstEvent(EventID::PatternSteps);
        if (ev && dynamic_cast<U32Event*>(ev))
            return static_cast<U32Event*>(ev)->value;
        return 0;
    }

    // ---- Track ----
    TrackInfoEvent* Track::getTrackInfoEvent() const
    {
        auto ev = m_tree.firstEvent(EventID::TrackInfo);
        return dynamic_cast<TrackInfoEvent*>(ev);
    }

    juce::String Track::getName() const
    {
        auto ev = m_tree.firstEvent(EventID::TrackName);
        if (ev) {
            if (auto* a = dynamic_cast<AsciiEvent*>(ev)) return a->value;
            if (auto* u = dynamic_cast<UnicodeEvent*>(ev)) return u->value;
        }
        return {};
    }
    void Track::setName(const juce::String& name)
    {
        m_tree.removeEvent(EventID::TrackName);
        std::unique_ptr<Event> ev;
        if (m_version >= FLVersion{ 11,5,0,0 })
            ev = std::make_unique<UnicodeEvent>(EventID::PluginName, name);
        else
            ev = std::make_unique<AsciiEvent>(EventID::PluginName, name);
        m_tree.addEvent(std::move(ev));
    }

    int Track::getIID() const
    {
        auto* ti = getTrackInfoEvent();
        if (ti) return ti->fields.iid;
        return -1;
    }

    bool Track::isMuted() const
    {
        auto* ti = getTrackInfoEvent();
        if (ti) return !ti->fields.enabled;
        return false;
    }
    void Track::setMuted(bool muted)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.enabled = !muted;
    }

    juce::Colour Track::getColor() const
    {
        auto* ti = getTrackInfoEvent();
        if (ti) return ti->fields.color;
        return juce::Colours::grey;
    }
    void Track::setColor(const juce::Colour& c)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.color = c;
    }

    uint32_t Track::getMotion() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.motion : 0;
    }
    void Track::setMotion(uint32_t motion)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.motion = motion;
    }
    uint32_t Track::getPress() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.press : 0;
    }
    void Track::setPress(uint32_t press)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.press = press;
    }
    uint32_t Track::getTriggerSync() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.triggerSync : 0;
    }
    void Track::setTriggerSync(uint32_t sync)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.triggerSync = sync;
    }
    bool Track::isQueued() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.queued : false;
    }
    void Track::setQueued(bool queued)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.queued = queued;
    }
    bool Track::isTolerant() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.tolerant : false;
    }
    void Track::setTolerant(bool tolerant)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.tolerant = tolerant;
    }
    uint32_t Track::getPositionSync() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.positionSync : 0;
    }
    void Track::setPositionSync(uint32_t sync)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.positionSync = sync;
    }
    bool Track::isGrouped() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.grouped : false;
    }
    void Track::setGrouped(bool grouped)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.grouped = grouped;
    }
    bool Track::isLocked() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.locked : false;
    }
    void Track::setLocked(bool locked)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.locked = locked;
    }
    float Track::getHeight() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.height : 0.0f;
    }
    void Track::setHeight(float height)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.height = height;
    }
    int32_t Track::getLockedHeight() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.lockedHeight : 0;
    }
    void Track::setLockedHeight(int32_t h)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.lockedHeight = h;
    }
    bool Track::getContentLocked() const
    {
        auto* ti = getTrackInfoEvent();
        return ti ? ti->fields.contentLocked : false;
    }
    void Track::setContentLocked(bool locked)
    {
        auto* ti = getTrackInfoEvent();
        if (ti) ti->fields.contentLocked = locked;
    }

    // ---- Arrangement ----
    int Arrangement::getIID() const
    {
        auto ev = m_tree.firstEvent(EventID::ArrangementNew);
        if (ev && dynamic_cast<U16Event*>(ev))
            return static_cast<U16Event*>(ev)->value;
        return -1;
    }
    juce::String Arrangement::getName() const
    {
        auto ev = m_tree.firstEvent(EventID::ArrangementName);
        if (ev) {
            if (auto* a = dynamic_cast<AsciiEvent*>(ev)) return a->value;
            if (auto* u = dynamic_cast<UnicodeEvent*>(ev)) return u->value;
        }
        return {};
    }
    void Arrangement::setName(const juce::String& name)
    {
        m_tree.removeEvent(EventID::ArrangementName);
        std::unique_ptr<Event> ev;
        if (m_version >= FLVersion{ 11,5,0,0 })
            ev = std::make_unique<UnicodeEvent>(EventID::PluginName, name);
        else
            ev = std::make_unique<AsciiEvent>(EventID::PluginName, name);
        m_tree.addEvent(std::move(ev));
    }

    std::vector<Track> Arrangement::getTracks() const
    {
        auto& trackTreeCache = m_project->getOrBuildTrackCache(m_arrangementIndex);
        // First call for this arrangement index: actually populate it, using
        // this Arrangement's own m_tree (Project doesn't have direct access
        // to figure out which events belong to which arrangement on its own).
        if (trackTreeCache.empty() && m_tree.hasEvent(EventID::TrackInfo))
        {
            auto trackInfoEvents = m_tree.getEvents(EventID::TrackInfo);
            auto trackNames = m_tree.getEvents(EventID::TrackName);
            size_t nameCursor = 0;
            for (auto* ev : trackInfoEvents) {
                EventTree sub;
                sub.addEvent(std::unique_ptr<Event>(ev->clone()));
                if (nameCursor < trackNames.size()) {
                    sub.addEvent(std::unique_ptr<Event>(trackNames[nameCursor]->clone()));
                    ++nameCursor;
                }
                trackTreeCache.push_back(std::move(sub));
            }
        }
        std::vector<Track> tracks;
        tracks.reserve(trackTreeCache.size());
        for (auto& tree : trackTreeCache)
            tracks.emplace_back(tree, m_version);
        return tracks;
    }

    std::vector<PlaylistItem> Arrangement::getPlaylistItems() const
    {
        auto ev = m_tree.firstEvent(EventID::Playlist);
        if (auto* pl = dynamic_cast<PlaylistEvent*>(ev))
            return pl->items;
        return {};
    }
    void Arrangement::setPlaylistItems(const std::vector<PlaylistItem>& items)
    {
        m_tree.removeEvent(EventID::Playlist);
        auto ev = std::make_unique<PlaylistEvent>();
        ev->items = items;
        ev->setItemSize(80);
        m_tree.addEvent(std::move(ev));
    }

    // ---- Slot ----
    juce::String Slot::getName() const
    {
        auto ev = m_tree.firstEvent(EventID::PluginName);
        if (ev) {
            if (auto* a = dynamic_cast<AsciiEvent*>(ev)) return a->value;
            if (auto* u = dynamic_cast<UnicodeEvent*>(ev)) return u->value;
        }
        return {};
    }
    void Slot::setName(const juce::String& name)
    {
        m_tree.removeEvent(EventID::PluginName);
        std::unique_ptr<Event> ev;
        if (m_version >= FLVersion{ 11,5,0,0 })
            ev = std::make_unique<UnicodeEvent>(EventID::PluginName, name);
        else
            ev = std::make_unique<AsciiEvent>(EventID::PluginName, name);
        m_tree.addEvent(std::move(ev));
    }
    bool Slot::isEnabled() const
    {
        auto ev = m_tree.firstEvent(EventID::IsEnabled);
        if (ev && dynamic_cast<BoolEvent*>(ev))
            return static_cast<BoolEvent*>(ev)->value;
        return false;
    }
    void Slot::setEnabled(bool enabled)
    {
        m_tree.removeEvent(EventID::IsEnabled);
        m_tree.addEvent(std::make_unique<BoolEvent>(EventID::IsEnabled, enabled));
    }
    juce::Colour Slot::getColor() const
    {
        auto ev = m_tree.firstEvent(EventID::Color);
        if (ev && dynamic_cast<ColorEvent*>(ev))
            return static_cast<ColorEvent*>(ev)->value;
        return juce::Colours::grey;
    }
    void Slot::setColor(const juce::Colour& c)
    {
        m_tree.removeEvent(EventID::Color);
        m_tree.addEvent(std::make_unique<ColorEvent>(EventID::Color, c));
    }

    // ---- Insert ----
    juce::String Insert::getName() const
    {
        auto ev = m_tree.firstEvent(EventID::PluginName);
        if (ev) {
            if (auto* a = dynamic_cast<AsciiEvent*>(ev)) return a->value;
            if (auto* u = dynamic_cast<UnicodeEvent*>(ev)) return u->value;
        }
        return {};
    }
    void Insert::setName(const juce::String& name)
    {
        m_tree.removeEvent(EventID::PluginName);
        std::unique_ptr<Event> ev;
        if (m_version >= FLVersion{ 11,5,0,0 })
            ev = std::make_unique<UnicodeEvent>(EventID::PluginName, name);
        else
            ev = std::make_unique<AsciiEvent>(EventID::PluginName, name);
        m_tree.addEvent(std::move(ev));
    }
    juce::Colour Insert::getColor() const
    {
        auto ev = m_tree.firstEvent(EventID::InsertColor);
        if (ev && dynamic_cast<ColorEvent*>(ev))
            return static_cast<ColorEvent*>(ev)->value;
        return juce::Colours::grey;
    }
    void Insert::setColor(const juce::Colour& c)
    {
        m_tree.removeEvent(EventID::InsertColor);
        m_tree.addEvent(std::make_unique<ColorEvent>(EventID::InsertColor, c));
    }
    bool Insert::isEnabled() const
    {
        auto ev = m_tree.firstEvent(EventID::IsEnabled);
        if (ev && dynamic_cast<BoolEvent*>(ev))
            return static_cast<BoolEvent*>(ev)->value;
        return true;
    }
    void Insert::setEnabled(bool enabled)
    {
        m_tree.removeEvent(EventID::IsEnabled);
        m_tree.addEvent(std::make_unique<BoolEvent>(EventID::IsEnabled, enabled));
    }
    std::vector<Slot> Insert::getSlots() const
    {
        auto& slotTreeCache = m_project->getOrBuildSlotCache(m_index);
        if (slotTreeCache.empty() && m_tree.hasEvent(EventID::SlotIID))
        {
            // The FLP format has no explicit "slot boundary" marker beyond
            // SlotIID itself, so we divide on that event.
            slotTreeCache = m_tree.divide(EventID::SlotIID, {
                EventID::SlotIID, EventID::IsEnabled, EventID::Color,
                EventID::NewPlugin, EventID::PluginParams, EventID::PluginName,
                EventID::PluginFactory, EventID::PluginIcon
            });
        }
        std::vector<Slot> slots;
        slots.reserve(slotTreeCache.size());
        int idx = 0;
        for (auto& tree : slotTreeCache)
            slots.emplace_back(tree, m_version, idx++);
        return slots;
    }

    // ---- Mixer ----
    std::vector<Insert> Mixer::getInserts() const
    {
        auto& insertTreeCache = m_project->getOrBuildInsertCache();
        if (insertTreeCache.empty() && m_tree.hasEvent(EventID::InsertOut))
        {
            // Individual mixer inserts have no explicit "new insert" event.
            // InsertColor seemed like a natural marker (every insert we'd
            // seen carried exactly one), but testing against a file with no
            // customized mixer state at all showed InsertColor is cosmetic -
            // it's only written for inserts that were actually touched, so
            // relying on it silently dropped every untouched insert's real
            // routing data (0 InsertColor events found, but 105 InsertOut
            // events - one per insert, Master included - sitting unclaimed).
            // InsertOut appears to be written unconditionally per insert
            // regardless of customization, so it's a more reliable marker.
            // Still a heuristic, not a documented format guarantee.
            insertTreeCache = m_tree.divide(EventID::InsertOut, {
                EventID::InsertColor, EventID::IsEnabled, EventID::InsertData,
                EventID::InsertIcon, EventID::InsertIn, EventID::InsertOut,
                EventID::InsertRouting, EventID::SlotIID, EventID::NewPlugin,
                EventID::PluginParams, EventID::PluginName, EventID::PluginFactory,
                EventID::PluginIcon, EventID::Color
            });
        }
        std::vector<Insert> inserts;
        inserts.reserve(insertTreeCache.size());
        int idx = 0;
        for (auto& tree : insertTreeCache)
            inserts.emplace_back(tree, m_version, m_project, idx++);
        return inserts;
    }
    bool Mixer::getAPDC() const
    {
        // The exact event carrying "automatic plugin delay compensation" has
        // not been identified so we avoid guessing
        // at (and potentially colliding with) an existing EventID.
        return false;
    }
    void Mixer::setAPDC(bool /*on*/)
    {
        // Not implemented - see getAPDC() note above.
    }

    // ---- Project load/save ----
    std::unique_ptr<Project> Project::load(const juce::File& file, juce::String* errorOut)
    {
        auto fail = [&](const juce::String& msg) -> std::unique_ptr<Project> {
            if (errorOut) *errorOut = msg;
            return nullptr;
        };

        juce::FileInputStream in(file);
        if (!in.openedOk()) return fail("Could not open file for reading.");

        char magic[4];
        if (in.read(magic, 4) != 4 || memcmp(magic, "FLhd", 4) != 0)
            return fail("Missing FLhd header magic - not an FLP file, or the file is truncated.");
        uint32_t headerSize; in.read(&headerSize, 4);
        int16_t format; in.read(&format, 2);
        uint16_t numChannels; in.read(&numChannels, 2);
        uint16_t ppq; in.read(&ppq, 2);

        if (in.read(magic, 4) != 4 || memcmp(magic, "FLdt", 4) != 0)
            return fail("Missing FLdt header magic after the header block.");
        uint32_t dataSize; in.read(&dataSize, 4);

        auto project = std::unique_ptr<Project>(new Project());
        project->m_ppq = ppq;
        auto tree = std::make_unique<EventTree>();

        FLVersion version{ 0,0,0,0 };
        juce::int64 dataStart = in.getPosition();
        juce::int64 dataEnd = dataStart + (juce::int64) dataSize;
        juce::int64 eventStartPos = dataStart;
        uint8_t idByte = 0;
        try
        {
            while (in.getPosition() < in.getTotalLength() && in.getPosition() < dataEnd)
            {
                eventStartPos = in.getPosition();
                if (in.read(&idByte, 1) != 1) break;
                EventID id = static_cast<EventID>(idByte);
                auto event = Event::read(in, id, version);

                // A variable-length event whose declared size runs past the
                // end of the FLdt chunk (or the file itself) means we've
                // desynced somewhere earlier - a fixed-size event upstream
                // almost certainly consumed the wrong number of bytes for
                // this file's format version. Fail loudly with the exact
                // position rather than silently reading garbage or throwing
                // deep inside Event::read.
                if (in.getPosition() > in.getTotalLength())
                {
                    return fail("Parse desync at byte " + juce::String(eventStartPos)
                        + " (event id " + juce::String((int) idByte) + "): a preceding event "
                        + "consumed the wrong number of bytes for this file's format version "
                        + (version.major > 0 ? ("(FLVersion " + version.toString() + ")") : juce::String())
                        + ". This usually means a newer FL Studio version changed the byte layout "
                        + "of an event this parser doesn't handle correctly yet.");
                }

                if (!event) continue;

                if (id == EventID::Version) {
                    if (auto* a = dynamic_cast<AsciiEvent*>(event.get())) {
                        auto parts = juce::StringArray::fromTokens(a->value, ".", "");
                        if (parts.size() >= 3) {
                            version.major = parts[0].getIntValue();
                            version.minor = parts[1].getIntValue();
                            version.patch = parts[2].getIntValue();
                            if (parts.size() >= 4) version.build = parts[3].getIntValue();
                        }
                    }
                }
                tree->addEvent(std::move(event));
            }
        }
        catch (const std::exception& e)
        {
            return fail(juce::String("Exception while parsing at byte ") + juce::String(eventStartPos)
                + " (event id " + juce::String((int) idByte) + "): " + e.what()
                + ". This usually means a preceding event consumed the wrong number of bytes for "
                + "this file's format version" + (version.major > 0 ? (" (FLVersion " + version.toString() + ")") : juce::String()) + ".");
        }
        project->m_version = version;
        project->m_eventTree = std::move(tree);
        return project;
    }

    std::unique_ptr<Project> Project::loadPartial(const juce::File& file, LoadReport& outReport)
    {
        outReport = LoadReport{};

        juce::FileInputStream in(file);
        if (!in.openedOk())
        {
            outReport.stopReason = "Could not open file for reading.";
            return nullptr;
        }

        char magic[4];
        if (in.read(magic, 4) != 4 || memcmp(magic, "FLhd", 4) != 0)
        {
            outReport.stopReason = "Missing FLhd header magic - not an FLP file, or the file is truncated.";
            return nullptr;
        }
        uint32_t headerSize; in.read(&headerSize, 4);
        int16_t format; in.read(&format, 2);
        uint16_t numChannels; in.read(&numChannels, 2);
        uint16_t ppq; in.read(&ppq, 2);

        if (in.read(magic, 4) != 4 || memcmp(magic, "FLdt", 4) != 0)
        {
            outReport.stopReason = "Missing FLdt header magic after the header block.";
            return nullptr;
        }
        uint32_t dataSize; in.read(&dataSize, 4);
        outReport.totalDataSize = (juce::int64) dataSize;

        auto project = std::unique_ptr<Project>(new Project());
        project->m_ppq = ppq;
        auto tree = std::make_unique<EventTree>();

        FLVersion version{ 0,0,0,0 };
        juce::int64 dataStart = in.getPosition();
        juce::int64 dataEnd = dataStart + (juce::int64) dataSize;
        juce::int64 eventStartPos = dataStart;
        uint8_t idByte = 0;

        auto stop = [&](const juce::String& reason)
        {
            outReport.fullyParsed = false;
            outReport.bytesParsed = juce::jmax((juce::int64) 0, eventStartPos - dataStart);
            outReport.stopReason = reason;
        };

        try
        {
            while (in.getPosition() < in.getTotalLength() && in.getPosition() < dataEnd)
            {
                eventStartPos = in.getPosition();
                if (in.read(&idByte, 1) != 1) break; // clean EOF, not an error - handled below
                EventID id = static_cast<EventID>(idByte);
                auto event = Event::read(in, id, version);

                if (in.getPosition() > in.getTotalLength())
                {
                    stop("Parse desync at byte " + juce::String(eventStartPos)
                        + " (event id " + juce::String((int) idByte) + "): a preceding event "
                        + "consumed the wrong number of bytes for this file's format version"
                        + (version.major > 0 ? (" (FLVersion " + version.toString() + ")") : juce::String())
                        + ". Everything before this point parsed cleanly and is included below.");
                    break;
                }

                if (!event) continue;

                if (id == EventID::Version) {
                    if (auto* a = dynamic_cast<AsciiEvent*>(event.get())) {
                        auto parts = juce::StringArray::fromTokens(a->value, ".", "");
                        if (parts.size() >= 3) {
                            version.major = parts[0].getIntValue();
                            version.minor = parts[1].getIntValue();
                            version.patch = parts[2].getIntValue();
                            if (parts.size() >= 4) version.build = parts[3].getIntValue();
                        }
                    }
                }
                tree->addEvent(std::move(event));
            }
        }
        catch (const std::exception& e)
        {
            stop(juce::String("Exception while parsing at byte ") + juce::String(eventStartPos)
                + " (event id " + juce::String((int) idByte) + "): " + e.what()
                + ". Everything before this point parsed cleanly and is included below.");
        }

        if (outReport.stopReason.isEmpty())
        {
            // Loop ended normally (clean EOF or reached dataEnd) rather than
            // via stop() - check whether we actually consumed the whole
            // declared stream.
            outReport.bytesParsed = juce::jmax((juce::int64) 0, in.getPosition() - dataStart);
            outReport.fullyParsed = (in.getPosition() >= dataEnd) || (in.getPosition() >= in.getTotalLength() && dataEnd > in.getTotalLength());
            if (!outReport.fullyParsed)
                outReport.stopReason = "Reached end of file before the declared data size (" +
                    juce::String(outReport.bytesParsed) + " of " + juce::String(outReport.totalDataSize) +
                    " bytes) - file is truncated. Everything before this point parsed cleanly and is included below.";
        }

        project->m_version = version;
        project->m_eventTree = std::move(tree);
        return project; // always returned, even when only partially parsed
    }

    void Project::save(const juce::File& file) const
    {
        juce::FileOutputStream out(file);
        if (!out.openedOk()) return;

        out.write("FLhd", 4);
        uint32_t headerSize = 6; out.write(&headerSize, 4);
        int16_t format = 0; out.write(&format, 2);
        uint16_t numChannels = getChannelCount(); out.write(&numChannels, 2);
        uint16_t ppq = m_ppq; out.write(&ppq, 2);

        out.write("FLdt", 4);
        uint32_t dataSizePos = out.getPosition();
        uint32_t dummy = 0; out.write(&dummy, 4);

        m_eventTree->writeAll(out);

        uint32_t endPos = out.getPosition();
        uint32_t totalDataSize = endPos - dataSizePos - 4;
        out.setPosition(dataSizePos);
        out.write(&totalDataSize, 4);
        out.setPosition(endPos);
    }

    // ---- Project getters ----
    Project::Metadata Project::getMetadata() const
    {
        Metadata md;
        auto getStr = [&](EventID id) -> juce::String {
            auto ev = m_eventTree->firstEvent(id);
            if (ev) {
                if (auto* a = dynamic_cast<AsciiEvent*>(ev)) return a->value;
                if (auto* u = dynamic_cast<UnicodeEvent*>(ev)) return u->value;
            }
            return {};
            };
        md.title = getStr(EventID::Title);
        md.author = getStr(EventID::Author);
        md.genre = getStr(EventID::Genre);
        md.comments = getStr(EventID::Comment);
        md.webUrl = getStr(EventID::URL);
        auto ev = m_eventTree->firstEvent(EventID::ShowInfo);
        if (ev && dynamic_cast<BoolEvent*>(ev))
            md.showInfoOnStart = static_cast<BoolEvent*>(ev)->value;
        return md;
    }

    Project::UserState Project::getUserState() const
    {
        UserState us;
        auto ev = m_eventTree->firstEvent(EventID::LoopActive);
        if (ev && dynamic_cast<BoolEvent*>(ev))
            us.playbackSong = static_cast<BoolEvent*>(ev)->value;
        ev = m_eventTree->firstEvent(EventID::Shuffle);
        if (ev && dynamic_cast<BoolEvent*>(ev))
            us.shuffle = static_cast<BoolEvent*>(ev)->value;
        ev = m_eventTree->firstEvent(EventID::CurrentPatNum);
        if (ev && dynamic_cast<U16Event*>(ev))
            us.pattern = static_cast<U16Event*>(ev)->value;
        return us;
    }

    double Project::getTempo() const
    {
        auto ev = m_eventTree->firstEvent(EventID::Tempo);
        if (ev && dynamic_cast<U32Event*>(ev))
            return static_cast<U32Event*>(ev)->value / 1000.0;

        // Very old FLP files predate the modern fixed-point Tempo(156) dword
        // event and only carry the deprecated TempoCoarse(66) word event,
        // storing BPM directly as an integer with no fixed-point scaling.
        auto legacyEv = m_eventTree->firstEvent(EventID::TempoCoarse);
        if (legacyEv && dynamic_cast<U16Event*>(legacyEv))
            return (double) static_cast<U16Event*>(legacyEv)->value;

        return 120.0;
    }
    void Project::setTempo(double bpm)
    {
        m_eventTree->removeEvent(EventID::Tempo);
        m_eventTree->addEvent(std::make_unique<U32Event>(EventID::Tempo, (uint32_t)(bpm * 1000)));
    }

    int Project::getChannelCount() const
    {
        auto evs = m_eventTree->getEvents(EventID::NewChan);
        return (int)evs.size();
    }

    int Project::getMainPitch() const
    {
        auto ev = m_eventTree->firstEvent(EventID::MainPitch);
        if (ev && dynamic_cast<I16Event*>(ev))
            return static_cast<I16Event*>(ev)->value;
        return 0;
    }
    void Project::setMainPitch(int cents)
    {
        m_eventTree->removeEvent(EventID::MainPitch);
        m_eventTree->addEvent(std::make_unique<I16Event>(EventID::MainPitch, (int16_t)cents));
    }

    std::vector<Channel*> Project::getChannels() const
    {
        if (!m_channelsLoaded)
        {
            m_channelTreeCache = m_eventTree->divide(EventID::NewChan, {
                EventID::IsEnabled, EventID::VolByte, EventID::PanByte, EventID::Zipped,
                EventID::ChanType, EventID::IsLocked, EventID::Color, EventID::PluginFactory,
                EventID::PluginName, EventID::NewPlugin, EventID::PluginParams,
                EventID::ChanParams, EventID::Levels, EventID::Polyphony, EventID::Tracking,
                EventID::LevelAdjusts, EventID::Automation, EventID::EnvelopeLFO,
                EventID::GroupNum, EventID::LayerFlags, EventID::SamplerFlags,
                EventID::FineTune, EventID::RootNote, EventID::CutCutBy, EventID::DelayReso,
                EventID::Reverb, EventID::Swing, EventID::Children,
                EventID::MIDIController, EventID::RemoteController, EventID::SampleFileName
                });
            m_channelObjCache.clear();
            for (auto& tree : m_channelTreeCache)
                m_channelObjCache.push_back(Channel::create(tree, m_version));
            m_channelsLoaded = true;
        }
        std::vector<Channel*> channels;
        channels.reserve(m_channelObjCache.size());
        for (auto& ch : m_channelObjCache)
            channels.push_back(ch.get());
        return channels;
    }

    std::vector<Pattern> Project::getPatterns() const
    {
        if (!m_patternsLoaded)
        {
            m_patternTreeCache = m_eventTree->divide(EventID::NewPat, {
                EventID::NewPat, EventID::PatName, EventID::PatternColor,
                EventID::PatternNotes, EventID::PatternCtrls, EventID::PatternSteps,
                EventID::Pattern157, EventID::Pattern158, EventID::PatternChanIID,
                EventID::Unknown161, EventID::Unknown162, EventID::Unknown163
                });
            m_patternsLoaded = true;
        }
        std::vector<Pattern> patterns;
        patterns.reserve(m_patternTreeCache.size());
        for (auto& tree : m_patternTreeCache)
            patterns.emplace_back(tree, m_version);
        return patterns;
    }

    Arrangement Project::getArrangement(int index) const
    {
        if (!m_arrangementsLoaded)
        {
            m_arrangementTreeCache = m_eventTree->divide(EventID::ArrangementNew, {
                EventID::ArrangementNew, EventID::ArrangementName, EventID::Playlist,
                EventID::TrackInfo, EventID::TrackName, EventID::MarkerPosition,
                EventID::MarkerText, EventID::TimeSigNumerator, EventID::TimeSigDenominator
                });

            // Pre-multi-arrangement FL versions have exactly one implicit
            // arrangement and never write an ArrangementNew marker at all -
            // so divide() (which only starts collecting after seeing its
            // separator) finds nothing, even though real TrackInfo/Playlist
            // data may exist directly in the top-level stream. Fall back to
            // treating the whole tree as that one implicit arrangement
            // rather than silently losing real data.
            if (m_arrangementTreeCache.empty())
            {
                auto implicit = m_eventTree->subtree([](const Event* e) {
                    auto id = e->id();
                    return id == EventID::Playlist || id == EventID::TrackInfo ||
                           id == EventID::TrackName || id == EventID::MarkerPosition ||
                           id == EventID::MarkerText;
                });
                if (implicit.size() > 0)
                    m_arrangementTreeCache.push_back(std::move(implicit));
            }

            m_arrangementsLoaded = true;
        }
        if (index >= 0 && index < (int)m_arrangementTreeCache.size())
            return Arrangement(m_arrangementTreeCache[(size_t)index], m_version, this, index);
        return Arrangement(m_emptyTree, m_version, this, -1);
    }

    Mixer Project::getMixer() const
    {
        if (!m_mixerLoaded)
        {
            m_mixerTreeCache = m_eventTree->subtree([](const Event* e) {
                auto id = e->id();
                return id == EventID::MixerBlob || id == EventID::InsertData ||
                    id == EventID::InsertRouting || id == EventID::InsertIcon ||
                    id == EventID::InsertColor || id == EventID::InsertIn ||
                    id == EventID::InsertOut ||
                    id == EventID::SlotIID || id == EventID::NewPlugin ||
                    id == EventID::PluginParams || id == EventID::PluginName ||
                    id == EventID::PluginFactory || id == EventID::PluginIcon ||
                    id == EventID::Color || id == EventID::IsEnabled;
                });
            m_mixerLoaded = true;
        }
        return Mixer(m_mixerTreeCache, m_version, this);
    }

    std::vector<EventTree>& Project::getOrBuildTrackCache(int arrangementIndex) const
    {
        static std::vector<EventTree> s_emptyStatic; // fallback-tree case (index < 0): no real arrangement, nothing to cache
        if (arrangementIndex < 0) return s_emptyStatic;

        if ((int)m_trackTreeCachePerArrangement.size() <= arrangementIndex)
            m_trackTreeCachePerArrangement.resize((size_t)arrangementIndex + 1);
        return m_trackTreeCachePerArrangement[(size_t)arrangementIndex];
    }

    std::vector<EventTree>& Project::getOrBuildInsertCache() const
    {
        return m_insertTreeCache;
    }

    std::vector<EventTree>& Project::getOrBuildSlotCache(int insertIndex) const
    {
        static std::vector<EventTree> s_emptyStatic;
        if (insertIndex < 0) return s_emptyStatic;

        if ((int)m_slotTreeCachePerInsert.size() <= insertIndex)
            m_slotTreeCachePerInsert.resize((size_t)insertIndex + 1);
        return m_slotTreeCachePerInsert[(size_t)insertIndex];
    }

    std::vector<RemoteControllerEvent*> Project::getAutomationChannels() const
    {
        std::vector<RemoteControllerEvent*> result;
        auto evs = m_eventTree->getEvents(EventID::RemoteController);
        for (auto* ev : evs) {
            if (auto* rce = dynamic_cast<RemoteControllerEvent*>(ev))
                result.push_back(rce);
        }
        return result;
    }

    std::vector<AutomationPoint> Project::getTempoAutomationPoints() const
    {
        std::vector<AutomationPoint> allPoints;
        auto autoEv = m_eventTree->firstEvent(EventID::Automation);
        if (auto* ae = dynamic_cast<AutomationEvent*>(autoEv)) {
            allPoints.insert(allPoints.end(), ae->points.begin(), ae->points.end());
        }
        return allPoints;
    }

    // =============================================================================
    // 9. Channel Sample Path & Tree Accessors
    // =============================================================================
    juce::String Channel::getSamplePath() const
    {
        auto ev = m_tree.firstEvent(EventID::SampleFileName);
        if (ev) {
            if (auto* a = dynamic_cast<AsciiEvent*>(ev)) return a->value;
            if (auto* u = dynamic_cast<UnicodeEvent*>(ev)) return u->value;
        }
        return {};
    }

    void Channel::setSamplePath(const juce::String& path)
    {
        m_tree.removeEvent(EventID::SampleFileName);
        std::unique_ptr<Event> ev;
        if (m_version >= FLVersion{ 11,5,0,0 })
            ev = std::make_unique<UnicodeEvent>(EventID::SampleFileName, path);
        else
            ev = std::make_unique<AsciiEvent>(EventID::SampleFileName, path);
        m_tree.addEvent(std::move(ev));
    }

    // =============================================================================
    // 10. Project Metadata & Mutable Tree
    // =============================================================================
    void Project::setMetadata(const Metadata& md)
    {
        auto setTextEvent = [&](EventID id, const juce::String& text) {
            m_eventTree->removeEvent(id);
            std::unique_ptr<Event> ev;
            if (m_version >= FLVersion{ 11,5,0,0 })
                ev = std::make_unique<UnicodeEvent>(id, text);
            else
                ev = std::make_unique<AsciiEvent>(id, text);
            m_eventTree->addEvent(std::move(ev));
            };

        setTextEvent(EventID::Title, md.title);
        setTextEvent(EventID::Author, md.author);
        setTextEvent(EventID::Genre, md.genre);
        setTextEvent(EventID::Comment, md.comments);
        setTextEvent(EventID::URL, md.webUrl);
        m_eventTree->removeEvent(EventID::ShowInfo);
        m_eventTree->addEvent(std::make_unique<BoolEvent>(EventID::ShowInfo, md.showInfoOnStart));
        m_metadata = md;
    }

} // namespace FL