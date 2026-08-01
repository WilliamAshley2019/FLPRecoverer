#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <functional>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace FL
{

    // =============================================================================
    // 1.  Event IDs – complete list from PyFLP, Kaitai, FlpEvents.h, and organiser
    // =============================================================================
    enum class EventID : uint8_t
    {
        // ----- BYTE range (0–63) -----
        IsEnabled = 0,
        NoteOn = 1,
        VolByte = 2,
        PanByte = 3,
        MIDIChan = 4,
        MIDINote = 5,
        MIDIPatch = 6,
        MIDIBank = 7,
        LoopActive = 9,
        ShowInfo = 10,
        Shuffle = 11,
        MainVol = 12,
        StretchByte = 13,
        Pitchable = 14,
        Zipped = 15,
        DelayFlags = 16,
        PatLength = 17,
        BlockLength = 18,
        UseLoopPoints = 19,
        LoopType = 20,
        ChanType = 21,
        MixSliceNum = 22,
        PanLaw = 23,
        Unknown24 = 24,
        Looped = 26,
        Licensed = 28,
        Unknown28 = 28,
        PlayTruncatedNotes = 30,
        IsLocked = 32,
        TimeSigNumerator = 33,
        TimeSigDenominator = 34,
        Unknown37 = 37,

        // ----- WORD range (64–127) -----
        NewChan = 64,   // Channel New
        NewPat = 65,   // Pattern New
        TempoCoarse = 66,   // Deprecated
        CurrentPatNum = 67,
        PatData = 68,
        FX = 69,
        FadeStereo = 70,
        CutOff = 71,
        DotVol = 72,
        DotPan = 73,
        PreAmp = 74,
        Decay = 75,
        Attack = 76,
        DotNote = 77,
        DotPitch = 78,
        DotMix = 79,
        MainPitch = 80,
        RandChan = 81,
        MixChan = 82,
        Resonance = 83,
        LoopBar = 84,
        StereoDelay = 85,
        FX3 = 86,
        DotReso = 87,
        DotCutOff = 88,
        ShiftDelay = 89,
        LoopEndBar = 90,
        Dot = 91,
        DotShift = 92,
        TempoFine = 93,   // Deprecated
        Children = 94,
        InsertIcon = 95,
        Swing = 97,
        SlotIID = 98,
        ArrangementNew = 99,   // New arrangement
        CurrentlySelected = 100,  // selected arrangement

        // ----- DWORD range (128–191) -----
        Color = 128,
        PlayListItem = 129,  // Deprecated? Actually PLAYLIST is 233
        Echo = 130,
        FXSine = 131,
        CutCutBy = 132,
        WindowHeight = 133,
        RootNote = 134,  // <-- Added for completeness
        MiddleNote = 135,
        Reserved = 136,
        MainResoCutOff = 137,
        DelayReso = 138,
        Reverb = 139,
        IntStretch = 140,
        SSNote = 141,
        FineTune = 142,
        SamplerFlags = 143,
        LayerFlags = 144,
        GroupNum = 145,
        CurCategory = 146,
        InsertOut = 147,
        MarkerPosition = 148,
        InsertColor = 149,
        PatternColor = 150,
        VerBuild = 151,  // build number from version string
        LoopPos = 152,
        AUSampleRate = 153,
        InsertIn = 154,
        PluginIcon = 155,
        Tempo = 156,
        Pattern157 = 157,
        Pattern158 = 158,
        VersionBuild = 159,
        PatternChanIID = 160,
        Unknown161 = 161,
        Unknown162 = 162,
        Unknown163 = 163,
        PatternSteps = 164,

        // ----- TEXT range (192–207) -----
        Undef = 192,
        Text = 192,
        PatName = 193,
        Title = 194,
        Comment = 195,
        SampleFileName = 196,
        URL = 197,
        CommentRTF = 198,
        Version = 199,
        Licensee = 200,
        PluginFactory = 201,
        DataPath = 202,
        PluginName = 203,
        Unknown204 = 204,
        MarkerText = 205,
        Genre = 206,
        Author = 207,
        MIDICtrls = 208,   // Actually DATA+0? We'll see
        Delay = 209,
        TS404Params = 210,
        DelayLine = 211,
        NewPlugin = 212,
        PluginParams = 213,
        ChanParams = 215,
        InitCtrls = 216,
        PLSelection = 217,
        EnvelopeLFO = 218,
        Levels = 219,
        Unknown220 = 220,
        Polyphony = 221,
        Unknown222 = 222,
        PatternCtrls = 223,
        PatternNotes = 224,
        MixerBlob = 225,
        MIDIController = 226,
        RemoteController = 227,
        Tracking = 228,
        LevelAdjusts = 229,
        Unknown230 = 230,
        CategoryName = 231,
        Unknown232 = 232,
        Playlist = 233,
        Automation = 234,
        InsertRouting = 235,
        InsertData = 236,
        Timestamp = 237,
        TrackInfo = 238,
        TrackName = 239,
        Unknown240 = 240,
        ArrangementName = 241,
    };

    // =============================================================================
    // 2.  Enums from Kaitai and PyFLP
    // =============================================================================
    enum class FilterType : uint32_t {
        FastLP = 0,
        LP = 1,
        BP = 2,
        HP = 3,
        BS = 4,
        LPx2 = 5,
        SVFLP = 6,
        SVFLPx2 = 7
    };

    enum class StretchMode : int32_t {
        Stretch = -1,
        Resample = 0,
        E3Generic = 1,
        E3Mono = 2,
        SliceStretch = 3,
        SliceMap = 4,
        Auto = 5,
        E2Generic = 6,
        E2Transient = 7,
        E2Mono = 8,
        E2Speech = 9
    };

    enum class ArpDirection : uint32_t {
        Off = 0,
        Up = 1,
        Down = 2,
        Bounce = 3,
        Sticky = 4,
        Random = 5
    };

    enum class DeclickMode : uint8_t {
        OutOnly = 0,
        NoBleed = 1,
        Transient = 2,
        Generic = 3,
        Smooth = 4,
        XFade = 5
    };

    enum class LFOShape : int32_t {
        Sine = 0,
        Triangle = 1,
        Pulse = 2
    };

    enum class TrackMotion : uint32_t {
        Stay = 0,
        OneShot = 1,
        MarchWrap = 2,
        MarchStay = 3,
        MarchStop = 4,
        Random = 5,
        ExclusiveRandom = 6
    };

    enum class TrackPress : uint32_t {
        Retrig = 0,
        HoldStop = 1,
        HoldMotion = 2,
        Latch = 3
    };

    enum class TrackSync : uint32_t {
        Off = 0,
        QuarterBeat = 1,
        HalfBeat = 2,
        Beat = 3,
        TwoBeats = 4,
        FourBeats = 5,
        Auto = 6
    };

    enum class PanLaw : uint8_t {
        Circular = 0,
        Triangular = 2
    };

    enum class ChannelType : uint8_t {
        Sampler = 0,
        Native = 2,
        Layer = 3,
        Instrument = 4,
        Automation = 5
    };

    // NOTE: not referenced anywhere else in this codebase - getCurveTypeName()/
    // getCurveTypeList() on AutomationEvent are the source of truth for curve
    // type values and are kept current there. Names here match FL's own
    // manual (11 real types: Single Curve, Double Curve, Alt Single Curve,
    // Alt Double Curve, Hold, Stairs, Smooth Stairs, Pulse, Wave, Half Sine,
    // Smooth) rather than the earlier pre-manual doc's guessed names
    // (Single/Double Curve "2"/"3", Flat Anchor), which don't correspond to
    // real FL concepts. CONFIRMED against real files via isolated
    // single-variable diffs: SingleCurve=0x00, DoubleCurve=0x01, Hold=0x02,
    // Stairs=0x03, SmoothStairs=0x04, Pulse=0x05, Wave=0x06, HalfSine=0x09.
    // Alt Single Curve / Alt Double Curve / Smooth remain completely
    // unmapped - no byte guessed here for them since guessing has been
    // wrong more often than right (Linear at 0x00, Pulse at 0x06, Wave at
    // 0x07, Half Sine at 0x08 were all guessed and all wrong).
    enum class AutomationCurveType : int32_t {
        SingleCurve = 0x00,
        DoubleCurve = 0x01,
        Hold = 0x02,
        Stairs = 0x03,
        SmoothStairs = 0x04,
        Pulse = 0x05,
        Wave = 0x06,
        HalfSine = 0x09
        // AltSingleCurve, AltDoubleCurve, Smooth: byte values unknown
    };

    // =============================================================================
    // 3.  Version helper
    // =============================================================================
    struct FLVersion
    {
        int major, minor, patch, build = 0;
        bool operator>=(const FLVersion& other) const;
        juce::String toString() const {
            return juce::String(major) + "." + juce::String(minor) + "." + juce::String(patch) + "." + juce::String(build);
        }
    };

    // =============================================================================
    // 4.  Event base classes
    // =============================================================================

    // Abstract base
    class Event
    {
    public:
        Event(EventID id) : m_id(id) {}
        virtual ~Event() = default;
        EventID id() const { return m_id; }

        virtual void write(juce::OutputStream& out) const = 0;
        virtual std::unique_ptr<Event> clone() const = 0;

        static std::unique_ptr<Event> read(juce::InputStream& in, EventID id, const FLVersion& version);

    protected:
        EventID m_id;
    };

    // ----- Fixed-size event classes -----
    class U8Event final : public Event {
    public:
        explicit U8Event(EventID eid, uint8_t v) : Event(eid), value(v) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<U8Event>(id(), value); }
        uint8_t value;
    };

    class BoolEvent final : public Event {
    public:
        explicit BoolEvent(EventID eid, bool v) : Event(eid), value(v) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<BoolEvent>(id(), value); }
        bool value;
    };

    class I16Event final : public Event {
    public:
        explicit I16Event(EventID eid, int16_t v) : Event(eid), value(v) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<I16Event>(id(), value); }
        int16_t value;
    };

    class U16Event final : public Event {
    public:
        explicit U16Event(EventID eid, uint16_t v) : Event(eid), value(v) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<U16Event>(id(), value); }
        uint16_t value;
    };

    class I32Event final : public Event {
    public:
        explicit I32Event(EventID eid, int32_t v) : Event(eid), value(v) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<I32Event>(id(), value); }
        int32_t value;
    };

    class U32Event final : public Event {
    public:
        explicit U32Event(EventID eid, uint32_t v) : Event(eid), value(v) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<U32Event>(id(), value); }
        uint32_t value;
    };

    class F32Event final : public Event {
    public:
        explicit F32Event(EventID eid, float v) : Event(eid), value(v) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<F32Event>(id(), value); }
        float value;
    };

    class ColorEvent final : public Event {
    public:
        explicit ColorEvent(EventID eid, const juce::Colour& c) : Event(eid), value(c) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<ColorEvent>(id(), value); }
        juce::Colour value;
    };

    class AsciiEvent final : public Event {
    public:
        explicit AsciiEvent(EventID eid, const juce::String& s) : Event(eid), value(s) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<AsciiEvent>(id(), value); }
        juce::String value;
    };

    class UnicodeEvent final : public Event {
    public:
        explicit UnicodeEvent(EventID eid, const juce::String& s) : Event(eid), value(s) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<UnicodeEvent>(id(), value); }
        juce::String value;
    };

    // ----- Structured event base (for events with fixed-layout payload) -----
    class StructEvent : public Event {
    public:
        StructEvent(EventID id) : Event(id) {}
        virtual void parse(juce::InputStream& in, size_t size) = 0;
        virtual void writeFields(juce::OutputStream& out) const = 0;
        void write(juce::OutputStream& out) const final;
    };

    // ----- List event base (for arrays of structs) -----
    class ListEvent : public Event {
    public:
        ListEvent(EventID id) : Event(id) {}
        virtual void parse(juce::InputStream& in, size_t size) = 0;
        virtual void writeItems(juce::OutputStream& out) const = 0;
        void write(juce::OutputStream& out) const final;
    };

    // ----- Unknown data event (fallback) -----
    class UnknownDataEvent final : public Event {
    public:
        UnknownDataEvent(EventID eid, const uint8_t* data, size_t size) : Event(eid), m_data(data, size) {}
        void write(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<UnknownDataEvent>(id(), static_cast<const uint8_t*>(m_data.getData()), m_data.getSize()); }
        juce::MemoryBlock m_data;
    };

    // =============================================================================
    // 5.  Specific structured events
    // =============================================================================

    // ---- TrackInfoEvent (event 238) ----
    struct TrackInfoEvent final : public StructEvent {
        TrackInfoEvent() : StructEvent(EventID::TrackInfo) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeFields(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<TrackInfoEvent>(*this); }

        struct Fields {
            uint32_t iid;
            juce::Colour color;
            uint32_t icon;
            bool enabled;
            float height;       // 0.0–1.0 (percentage)
            int32_t lockedHeight;
            bool contentLocked;
            uint32_t motion;    // TrackMotion
            uint32_t press;     // TrackPress
            uint32_t triggerSync; // TrackSync
            bool queued;
            bool tolerant;
            uint32_t positionSync; // TrackSync
            bool grouped;
            bool locked;
        } fields;
    };

    // ---- PlaylistEvent (event 233) ----
    struct PlaylistItem {
        uint32_t position;
        uint16_t patternBase; // always 20480
        uint16_t itemIndex;
        uint32_t length;
        uint16_t trackRvidx;  // reversed track index
        uint16_t group;
        uint16_t itemFlags;
        float startOffset;
        float endOffset;
    };

    class PlaylistEvent final : public ListEvent {
    public:
        PlaylistEvent() : ListEvent(EventID::Playlist) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeItems(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<PlaylistEvent>(*this); }

        int getItemSize() const { return m_itemSize; }
        void setItemSize(int size) { m_itemSize = size; }

        std::vector<PlaylistItem> items;
    private:
        int m_itemSize = 80; // default for FL25
        static int detectItemSize(const uint8_t* data, size_t totalSize, int offset);
    };

    // ---- LevelsEvent (event 219) ----
    struct LevelsEvent final : public StructEvent {
        LevelsEvent() : StructEvent(EventID::Levels) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeFields(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<LevelsEvent>(*this); }

        struct Fields {
            std::optional<uint32_t> pan;      // 0–12800, centre=6400
            std::optional<uint32_t> volume;   // 0–12800
            std::optional<int32_t> pitchShift; // -4800..4800 cents
            std::optional<uint32_t> filterModX;
            std::optional<uint32_t> filterModY;
            std::optional<uint32_t> filterType; // FilterType
        } fields;
    };

    // ---- ChannelBlobEvent (event 215) – raw payload with accessors ----
    class ChannelBlobEvent final : public StructEvent {
    public:
        ChannelBlobEvent() : StructEvent(EventID::ChanParams) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeFields(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<ChannelBlobEvent>(*this); }

        juce::MemoryBlock rawData;

        // Accessors for common fields (offsets from Kaitai)
        bool getNoDC() const;
        void setNoDC(bool val);
        uint8_t getDelayFlags() const;
        void setDelayFlags(uint8_t flags);
        bool getUseMainPitch() const;
        void setUseMainPitch(bool val);
        uint32_t getArpDirection() const;
        void setArpDirection(uint32_t dir);
        // ... add more as needed
    };

    // ---- RemoteControllerEvent (event 227) – automation channel routing ----
    struct RemoteControllerEvent final : public StructEvent {
        RemoteControllerEvent() : StructEvent(EventID::RemoteController) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeFields(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<RemoteControllerEvent>(*this); }

        struct Fields {
            uint16_t unknown1;   // observed 0
            uint32_t trackId;    // channel IID or 0
            uint16_t unknown2;   // observed 0
            uint16_t paramId;    // 5 = tempo, 0 = volume, 2 = pitch
            uint16_t destId;     // 0x4000 = master, 0x2000-0x3FFF = mixer track
            uint32_t unknown3;   // observed 0
            uint32_t unknown4;   // observed 0
        } fields;
    };

    // ---- AutomationEvent (event 234) – automation points ----
   // ---- AutomationEvent (event 234) – automation points ----
// Legacy point structure (kept for backward compatibility)
    struct AutomationPoint {
        double beatIncrement; // delta from previous point in beats
        double value;         // raw automation value (0..1 usually)
        float tension;        // 0..1
        uint8_t unknown3[3];  // usually 0
        uint8_t direction;    // 0 or 1
    };

    class AutomationEvent final : public ListEvent {
    public:
        AutomationEvent() : ListEvent(EventID::Automation) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeItems(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<AutomationEvent>(*this); }

        // ============================================================
        // Complete Record structure - CONFIRMED against 7 real test files
        // (2-point, 4-point, 7-point, and FL11-vs-FL26 tension-isolated
        // pairs), covering position, value, tension, and 3 of the real
        // curve-type values.
        // ============================================================
        //
        // IMPORTANT - this is NOT a flat per-point struct on disk. Each
        // 24-byte slot in the file straddles TWO points:
        //   - bytes  8-15 (position) and 16-23 (value) belong to point (k+1)
        //   - bytes  0-3  (tension, float) and byte 4 (curveType) belong
        //     to point k itself - i.e. one slot LATER than that point's
        //     own position/value.
        // This means a clip's real point data always needs one MORE
        // 24-byte slot than its point count: the last point's own
        // position/value live in slot (P-1), but its tension/curveType
        // live in slot P, whose position/value bytes are unused (and read
        // as NaN under the naive fixed-stride read that's how this was
        // first noticed). parse() below reads that extra slot for tension/
        // curveType only and does not treat it as an extra point.
        //
        // controlCode only carries real meaning on the very first slot
        // (marks the start point) - preserved verbatim via startMarkerBytes
        // rather than reinterpreted, since we don't know what its second
        // int32 (values 2/4/7 seen across test files) means yet.
        //
        // curveType confirmed values so far: 0 = Single Curve, 1 = Double
        // Curve, 5 = Pulse. Others are still the ORIGINAL reverse-engineering
        // doc's guesses and are very possibly wrong (that doc's guessed
        // Single Curve=0x02 and Linear=0x00 were both wrong) - treat
        // anything not in {0,1,5} as unconfirmed.
        //
        // tension confirmed as a signed float, -1.0 to +1.0 (-100% to
        // +100%), isolated via a single-variable before/after file pair.
        // Sign is presumably which side of the segment the curve bulges
        // toward; exact interpolation math using it is NOT confirmed,
        // only its storage location/range.
        //
        // Pulse's additional "step count" parameter (confirmed to exist -
        // e.g. "4 steps" vs "14 steps" on two real Pulse segments) has NOT
        // been located yet; it isn't in this record at all as far as we've
        // found. Rendered as a plain Hold for now (see AutomationCurveView).
        struct Record {
            int32_t controlCode;   // only meaningful on records[0]; opaque elsewhere, see startMarkerBytes
            int32_t curveType;     // confirmed: 0=Single Curve, 1=Double Curve, 5=Pulse; others unconfirmed
            double position;       // delta in BEATS from the previous point (not absolute, not PPQ ticks)
            double value;          // normalized 0.0-1.0 fraction of the destination parameter's own range
            float tension = 0.0f;  // signed, -1.0..+1.0; meaningless on records[0] (start has no incoming curve)
        };

        // Get number of steps/subdivisions
        int getStepCount() const {
            if (records.size() < 2) return 0;
            return (int)records.size() - 1;
        }

        // Step count from tension, for the curve types where FL's manual
        // confirms tension is repurposed as a step/frequency control
        // (Stairs, Smooth Stairs, Pulse, Wave) rather than steepness.
        // Fit against 18 REAL manually-counted (tension%, step count) pairs
        // on Stairs specifically: 100-92%=0 (flat), 91-79%=2 (plateau),
        // then steps = round(2.32 * exp(0.0684 * (79 - tensionPercent)))
        // below 79%. Matches the counted data closely (exact or within 1
        // step almost everywhere tested, up to 46% where counting by eye
        // got difficult). NOT yet confirmed for Pulse/Wave/Smooth Stairs
        // specifically - applied here on the assumption they share the
        // same underlying frequency-scaling mechanism the manual describes
        // for all four, but only Stairs has been checked against real
        // counted data.
        static int getStepCountForTension(float tension) {
            float tensionPercent = std::abs(tension) * 100.0f;
            if (tensionPercent >= 92.0f) return 0;
            if (tensionPercent >= 79.0f) return 2;
            double x = 79.0 - tensionPercent;
            double steps = 2.32 * std::exp(0.0684 * x);
            return (int)std::round(steps);
        }

        // Get curve type for a specific segment (0 = first segment)
        int getCurveTypeForSegment(int segmentIndex) const {
            if (segmentIndex < 0 || segmentIndex >= (int)records.size() - 1) return 0;
            return records[segmentIndex + 1].curveType;
        }

        // Set curve type for a specific segment (0 = first segment)
        void setCurveTypeForSegment(int segmentIndex, int curveType) {
            if (segmentIndex < 0 || segmentIndex >= (int)records.size() - 1) return;
            records[segmentIndex + 1].curveType = curveType;
        }

        // Set curve type for all interior segments
        void setCurveTypeForAllSegments(int curveType) {
            for (size_t i = 1; i < records.size(); ++i) {
                records[i].curveType = curveType;
            }
        }

        // Get human-readable curve type name. Full 11-type list per FL's
        // own manual (Right-Click menu on a control point); byte values for
        // only 0/1/5 are CONFIRMED against real files (see the comment on
        // Record) - the rest are placed at their doc-guessed byte value but
        // that mapping is NOT verified, and the doc's guesses for 0x00 and
        // 0x02 were already shown wrong once. Treat anything not in {0,1,5}
        // as "we know this type exists and roughly what it does, but not
        // which byte value FL uses for it."
        static juce::String getCurveTypeName(int curveType) {
            switch (curveType) {
            case 0x00: return "Single Curve";    // CONFIRMED byte value
            case 0x01: return "Double Curve";    // CONFIRMED byte value
            case 0x02: return "Hold";            // CONFIRMED byte value
            case 0x03: return "Stairs";          // CONFIRMED byte value
            case 0x04: return "Smooth Stairs";   // CONFIRMED byte value
            case 0x05: return "Pulse";           // CONFIRMED byte value
            case 0x06: return "Wave";            // CONFIRMED byte value
            case 0x09: return "Half Sine";       // CONFIRMED byte value
            default: return "Unknown (" + juce::String(curveType) + ")";
            }
        }

        // Get all curve type names for UI dropdowns. Only the first 3 are
        // confirmed - see getCurveTypeName.
        // Only lists curve types with a CONFIRMED byte value. Three real
        // FL curve types have no byte mapped yet at all - Alt Single Curve,
        // Alt Double Curve, and Smooth - deliberately omitted rather than
        // guessed, since guessed bytes in this table have been wrong more
        // often than right.
        static std::vector<std::pair<int, juce::String>> getCurveTypeList() {
            return {
                {0x00, "Single Curve"},
                {0x01, "Double Curve"},
                {0x02, "Hold"},
                {0x03, "Stairs"},
                {0x04, "Smooth Stairs"},
                {0x05, "Pulse"},
                {0x06, "Wave"},
                {0x09, "Half Sine"}
            };
        }

        // Legacy points (kept for backward compatibility)
        std::vector<AutomationPoint> points;

        // NEW: Complete records (replaces points for modern FLP files)
        std::vector<Record> records;

        // Real-file testing (see comment on parse()) found that dividing
        // (payloadSize - HEADER_SIZE) by POINT_SIZE overclaims: only the
        // first couple of "records" for a genuine 2-point clip decoded to
        // sane values: bytes past that point failed basic sanity checks
        // (curveType outside 0x00-0x0C, controlCode not one of 0/3/-1) and
        // are something else entirely - almost certainly the "PluginSettings
        // (binary blob)" mentioned in the original reverse-engineering notes,
        // not more Records. Preserved verbatim and rewritten unchanged so
        // Save Project As doesn't corrupt files we don't fully understand
        // yet. Still needs a file with 3+ real points to confirm where the
        // real cutoff actually is.
        juce::MemoryBlock trailingData;

        // Slot 0's bytes 0-7, verbatim. Consistently starts with controlCode
        // 3 (marking the start point) but its second int32 varies across
        // real files (2, 4, 7 seen) with unknown meaning - preserved as-is
        // rather than guessed at, so edits never corrupt it.
        juce::MemoryBlock startMarkerBytes;

        // The 13-byte header, captured verbatim at parse time. writeItems()
        // used to hardcode a *different*, longer (16-byte) fixed pattern
        // that doesn't match what real files on disk actually contain -
        // that mismatch alone would desync every byte after this event on
        // Save Project As. Round-tripping the real bytes sidesteps needing
        // to fully decode this header's meaning for now.
        juce::MemoryBlock headerBytes;

    private:
        static constexpr size_t POINT_SIZE = 24; // 8+8+4+3+1
        static constexpr size_t HEADER_SIZE = 13; // 13-byte header before records
    };

    // ---- MixerBlobEvent (event 225) ----
    struct MixerParam {
        uint8_t id;          // param ID (slot_on, slot_mix, vol, pan, etc.)
        uint8_t _u1;
        uint16_t channelData; // packed insert/slot index
        int32_t msg;
    };
    class MixerBlobEvent final : public ListEvent {
    public:
        MixerBlobEvent() : ListEvent(EventID::MixerBlob) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeItems(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<MixerBlobEvent>(*this); }
        std::vector<MixerParam> params;
    };

    // ---- WrapperEvent (event 212) ----
    struct WrapperEvent final : public StructEvent {
        WrapperEvent() : StructEvent(EventID::NewPlugin) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeFields(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<WrapperEvent>(*this); }

        struct Fields {
            uint16_t flags;    // bitfield (visible, detached, demo, etc.)
            uint8_t page;      // 0=editor, 1=settings, 3=sample, 4=envlfo, 5=misc
            uint32_t width;
            uint32_t height;
        } fields;
    };

    // ---- PatternNotesEvent (event 224) ----
    struct Note {
        uint32_t position;
        uint16_t flags;      // bit 3 = slide
        uint16_t channelIID;
        uint32_t length;
        uint16_t key;        // 0..131 (C0..B10)
        uint16_t group;
        uint8_t finePitch;   // 0..240, default 120
        uint8_t release;     // 0..128, default 64
        uint8_t midiChannel; // 0..15 color, +128 for MIDI drag
        uint8_t pan;         // 0..128, default 64
        uint8_t velocity;    // 0..128, default 100
        uint8_t modX;        // default 128
        uint8_t modY;        // default 128
    };
    class PatternNotesEvent final : public ListEvent {
    public:
        PatternNotesEvent() : ListEvent(EventID::PatternNotes) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeItems(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<PatternNotesEvent>(*this); }
        std::vector<Note> notes;
    };

    // ---- PatternCtrlsEvent (event 223) ----
    struct Controller {
        uint32_t position;
        uint8_t channelIID;
        uint8_t flags;
        float value;
    };
    class PatternCtrlsEvent final : public ListEvent {
    public:
        PatternCtrlsEvent() : ListEvent(EventID::PatternCtrls) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeItems(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<PatternCtrlsEvent>(*this); }
        std::vector<Controller> controllers;
    };

    // ---- InsertDataEvent (event 236) ----
    struct InsertDataEvent final : public StructEvent {
        InsertDataEvent() : StructEvent(EventID::InsertData) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeFields(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<InsertDataEvent>(*this); }
        uint32_t flags = 0; // bitfield
    };

    // ---- InsertRoutingEvent (event 235) ----
    class InsertRoutingEvent final : public ListEvent {
    public:
        InsertRoutingEvent() : ListEvent(EventID::InsertRouting) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeItems(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<InsertRoutingEvent>(*this); }
        std::vector<bool> routes;
    };

    // ---- TimestampEvent (event 237) ----
    struct TimestampEvent final : public StructEvent {
        TimestampEvent() : StructEvent(EventID::Timestamp) {}
        void parse(juce::InputStream& in, size_t size) override;
        void writeFields(juce::OutputStream& out) const override;
        std::unique_ptr<Event> clone() const override { return std::make_unique<TimestampEvent>(*this); }
        double createdOn = 0.0;   // days since 1899-12-30
        double timeSpent = 0.0;   // days
    };

    // =============================================================================
    // 6.  EventTree – container with mutable views
    // =============================================================================

    class EventTree
    {
    public:
        EventTree() = default;
        EventTree(const EventTree&) = delete;
        EventTree& operator=(const EventTree&) = delete;
        EventTree(EventTree&&) noexcept = default;
        EventTree& operator=(EventTree&&) noexcept = default;

        void addEvent(std::unique_ptr<Event> event);
        void removeEvent(EventID id, int index = 0);
        std::vector<Event*> getEvents(EventID id) const;
        Event* firstEvent(EventID id) const;
        bool hasEvent(EventID id) const;
        size_t size() const { return m_events.size(); }

        // Grouping methods (like PyFLP)
        std::vector<EventTree> divide(EventID separator, const std::vector<EventID>& allowed) const;
        std::vector<EventTree> separate(EventID id) const;
        EventTree subtree(std::function<bool(const Event*)> predicate) const;

        // Serialise all events in order
        void writeAll(juce::OutputStream& out) const;

    private:
        std::vector<std::unique_ptr<Event>> m_events;
    };

    // =============================================================================
    // 7.  High-level models
    // =============================================================================

    // Forward declarations
    class Project;
    class Channel;
    class Pattern;
    class Arrangement;
    class Track;
    class Mixer;
    class Insert;
    class Slot;

    // ---- Channel ----
    class Channel
    {
    public:
        virtual ~Channel() = default;
        static std::unique_ptr<Channel> create(EventTree& tree, const FLVersion& version);

        // Identity
        int getIID() const;
        juce::String getName() const;
        void setName(const juce::String& name);
        juce::String getInternalName() const;
        ChannelType getType() const;
        void setType(ChannelType type);

        // Color
        juce::Colour getColor() const;
        void setColor(const juce::Colour& c);

        // State
        bool isEnabled() const;
        void setEnabled(bool enabled);
        bool isZipped() const;
        void setZipped(bool zipped);
        bool isLocked() const;
        void setLocked(bool locked);

        // Volume & Pan
        std::optional<int> getVolume() const;
        void setVolume(int vol);
        std::optional<int> getPan() const;
        void setPan(int pan);
        std::optional<int> getPitchShift() const;
        void setPitchShift(int cents);

        // Filter
        std::optional<int> getFilterCutoff() const;
        void setFilterCutoff(int value);
        std::optional<int> getFilterResonance() const;
        void setFilterResonance(int value);
        std::optional<int> getFilterType() const;
        void setFilterType(int type);

        // Group (display group index)
        int getGroupIndex() const;
        void setGroupIndex(int group);

        // ===== PATCHES FOR flphelper.cpp =====
        // Sample path accessors (Required by Sample Scanner & Batch Processor)
        juce::String getSamplePath() const;
        void setSamplePath(const juce::String& path);

        // EventTree accessors (Required by Plugin Inspector, Stats Generator, Automation Editor)
        const EventTree& getEventTree() const { return m_tree; }
        EventTree& getMutableTree() { return m_tree; }

    protected:
        Channel(EventTree& tree, const FLVersion& version) : m_tree(tree), m_version(version) {}
        EventTree& m_tree;
        FLVersion m_version;

        LevelsEvent* getLevelsEvent() const;
        void ensureLevelsEvent();
    };

    // ---- Pattern ----
    class Pattern
    {
    public:
        Pattern(EventTree& tree, const FLVersion& version) : m_tree(tree), m_version(version) {}
        int getIID() const;
        juce::String getName() const;
        void setName(const juce::String& name);
        juce::Colour getColor() const;
        void setColor(const juce::Colour& c);
        std::vector<Note> getNotes() const;
        void setNotes(const std::vector<Note>& notes);
        std::vector<Controller> getControllers() const;
        void setControllers(const std::vector<Controller>& ctrls);
        int getLength() const; // in PPQ ticks

    private:
        EventTree& m_tree;
        FLVersion m_version;
    };

    // ---- Track ----
    class Track
    {
    public:
        Track(EventTree& tree, const FLVersion& version) : m_tree(tree), m_version(version) {}
        juce::String getName() const;
        void setName(const juce::String& name);
        int getIID() const;
        bool isMuted() const;
        void setMuted(bool muted);
        juce::Colour getColor() const;
        void setColor(const juce::Colour& c);
        uint32_t getMotion() const;
        void setMotion(uint32_t motion);
        uint32_t getPress() const;
        void setPress(uint32_t press);
        uint32_t getTriggerSync() const;
        void setTriggerSync(uint32_t sync);
        bool isQueued() const;
        void setQueued(bool queued);
        bool isTolerant() const;
        void setTolerant(bool tolerant);
        uint32_t getPositionSync() const;
        void setPositionSync(uint32_t sync);
        bool isGrouped() const;
        void setGrouped(bool grouped);
        bool isLocked() const;
        void setLocked(bool locked);
        float getHeight() const;
        void setHeight(float height);
        int32_t getLockedHeight() const;
        void setLockedHeight(int32_t h);
        bool getContentLocked() const;
        void setContentLocked(bool locked);

    private:
        EventTree& m_tree;
        FLVersion m_version;
        TrackInfoEvent* getTrackInfoEvent() const;
    };

    // ---- Arrangement ----
    class Arrangement
    {
    public:
        Arrangement(EventTree& tree, const FLVersion& version, const Project* project, int arrangementIndex)
            : m_tree(tree), m_version(version), m_project(project), m_arrangementIndex(arrangementIndex) {}
        int getIID() const;
        juce::String getName() const;
        void setName(const juce::String& name);
        // Track subtrees are cached on the owning Project, not here, so
        // Arrangement is safe to use as a temporary (e.g.
        // project->getArrangement(0).getTracks() works correctly) - only
        // Project itself needs to outlive the returned Track objects.
        std::vector<Track> getTracks() const;
        std::vector<PlaylistItem> getPlaylistItems() const;
        void setPlaylistItems(const std::vector<PlaylistItem>& items);
        // Time markers not fully implemented

    private:
        EventTree& m_tree;
        FLVersion m_version;
        const Project* m_project;
        int m_arrangementIndex;
    };

    // ---- Mixer ----
    class Slot
    {
    public:
        Slot(EventTree& tree, const FLVersion& version, int index = 0) : m_tree(tree), m_version(version), m_index(index) {}
        int getIndex() const { return m_index; }
        juce::String getName() const;
        void setName(const juce::String& name);
        bool isEnabled() const;
        void setEnabled(bool enabled);
        juce::Colour getColor() const;
        void setColor(const juce::Colour& c);
        // Plugin data not implemented
    private:
        EventTree& m_tree;
        FLVersion m_version;
        int m_index;
    };

    class Insert
    {
    public:
        Insert(EventTree& tree, const FLVersion& version, const Project* project, int index = 0)
            : m_tree(tree), m_version(version), m_project(project), m_index(index) {}
        int getIID() const { return m_index; } // inserts have no explicit IID event; index of appearance is used
        juce::String getName() const;
        void setName(const juce::String& name);
        juce::Colour getColor() const;
        void setColor(const juce::Colour& c);
        // Slot subtrees are cached on the owning Project - see the note on
        // Arrangement above; the same reasoning applies here.
        std::vector<Slot> getSlots() const;
        bool isEnabled() const;
        void setEnabled(bool enabled);
        // more insert properties
    private:
        EventTree& m_tree;
        FLVersion m_version;
        const Project* m_project;
        int m_index;
    };

    class Mixer
    {
    public:
        Mixer(EventTree& tree, const FLVersion& version, const Project* project)
            : m_tree(tree), m_version(version), m_project(project) {}
        // Insert subtrees are cached on the owning Project - see the note on
        // Arrangement above; the same reasoning applies here.
        std::vector<Insert> getInserts() const;
        bool getAPDC() const;
        void setAPDC(bool on);
    private:
        EventTree& m_tree;
        FLVersion m_version;
        const Project* m_project;
    };

    // ---- Project ----
    class Project
    {
    public:
        static std::unique_ptr<Project> load(const juce::File& file, juce::String* errorOut = nullptr);

        // For forensic/recovery use: a corrupted or partially-recovered file
        // (e.g. from a disk-recovery tool) will often parse cleanly for a
        // while and then desync or hit EOF partway through. load() discards
        // everything in that case and returns nullptr, which throws away
        // whatever WAS successfully parsed - fine for normal use, but not
        // what you want when the file is already known to be damaged and
        // you're trying to salvage whatever content survived.
        //
        // loadPartial() runs the identical parse loop but keeps the partial
        // event tree instead of discarding it, and fills in `outReport` with
        // exactly where parsing stopped and why. Every existing extraction
        // tool (StatsGenerator, MidiBridge, SampleScanner, ArrangementDumper)
        // already just queries "whatever's in the tree" with no assumption
        // of completeness, so they work against the result unmodified - a
        // pattern that only half-parsed still yields whatever notes/patterns
        // fell before the cutoff.
        struct LoadReport
        {
            bool fullyParsed = false;
            juce::int64 bytesParsed = 0;   // bytes of the FLdt event stream successfully consumed
            juce::int64 totalDataSize = 0; // FLdt's own declared size
            juce::String stopReason;       // empty if fullyParsed
        };
        static std::unique_ptr<Project> loadPartial(const juce::File& file, LoadReport& outReport);

        void save(const juce::File& file) const;

        // Metadata
        struct Metadata {
            juce::String title;
            juce::String author;
            juce::String genre;
            juce::String comments;
            juce::String webUrl;
            bool showInfoOnStart = true;
        };
        struct UserState {
            bool playbackSong = false;
            bool shuffle = false;
            uint16_t pattern = 0;
        };
        Metadata getMetadata() const;
        UserState getUserState() const;

        // Version and global settings
        FLVersion getVersion() const { return m_version; }
        int getPPQ() const { return m_ppq; }
        double getTempo() const;
        void setTempo(double bpm);
        int getChannelCount() const;
        int getMainPitch() const;
        void setMainPitch(int cents);

        // Access to models
        std::vector<Channel*> getChannels() const; // raw pointers, owned by EventTree
        std::vector<Pattern> getPatterns() const;
        Arrangement getArrangement(int index = 0) const;
        Mixer getMixer() const;

        // Utilities
        std::vector<RemoteControllerEvent*> getAutomationChannels() const;
        std::vector<AutomationPoint> getTempoAutomationPoints() const;

        // ===== PATCHES FOR flphelper.cpp =====
        // Metadata setter (Required by Batch Processor)
        void setMetadata(const Metadata& md);
        // Mutable tree accessor (Required by Cleaner to actually delete unused events)
        EventTree& getMutableEventTree() { return *m_eventTree; }

    private:
        Project() = default;
        std::unique_ptr<EventTree> m_eventTree;
        FLVersion m_version;
        int m_ppq = 96;
        Metadata m_metadata;
        UserState m_userState;

        // Lazy caches. Project must stay alive as long as anything returned
        // from getChannels()/getPatterns()/getArrangement()/getMixer() (or
        // objects derived from them, like Track/Insert/Slot) is in use - see
        // NOTE comments on those methods.
        mutable std::vector<EventTree> m_channelTreeCache;
        mutable std::vector<std::unique_ptr<Channel>> m_channelObjCache;
        mutable bool m_channelsLoaded = false;

        mutable std::vector<EventTree> m_patternTreeCache;
        mutable bool m_patternsLoaded = false;

        mutable std::vector<EventTree> m_arrangementTreeCache;
        mutable bool m_arrangementsLoaded = false;

        mutable EventTree m_mixerTreeCache;
        mutable bool m_mixerLoaded = false;

        mutable EventTree m_emptyTree; // stable fallback for out-of-range arrangement lookups

        // Track/Insert/Slot sub-caches, owned here rather than by Arrangement/
        // Mixer/Insert themselves. Those are lightweight view objects very
        // naturally used as temporaries (e.g. project->getArrangement(0).getTracks()
        // reads perfectly reasonably) - if they owned their own sub-caches,
        // that pattern would silently return dangling references the moment
        // the temporary is destroyed. Owning the caches here instead means
        // any number of temporary Arrangement/Mixer/Insert objects can be
        // constructed and discarded freely; only Project itself needs to
        // outlive the Track/Insert/Slot objects derived from it.
    public:
        std::vector<EventTree>& getOrBuildTrackCache(int arrangementIndex) const;
        std::vector<EventTree>& getOrBuildInsertCache() const;
        std::vector<EventTree>& getOrBuildSlotCache(int insertIndex) const;
    private:
        mutable std::vector<std::vector<EventTree>> m_trackTreeCachePerArrangement;

        mutable std::vector<EventTree> m_insertTreeCache;

        mutable std::vector<std::vector<EventTree>> m_slotTreeCachePerInsert;
    };

} // namespace FL