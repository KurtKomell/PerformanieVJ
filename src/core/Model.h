#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QUuid>
#include <optional>

namespace pvj::core {

// Enums

enum class BankSetType : int {
    TypeA = 0,  // GrandVJ "Type 0" - main cells
    TypeB = 1,  // GrandVJ "Type 1" - secondary cells
};

enum class VisualType {
    Empty,
    Media,       // reference to MediaItem by uuid
    Generator,   // built-in source (test pattern, spout, ...)
    /// Post-composite filter preset: trigger applies filterChain to the full mixer (no mix layer).
    MixerFilter,
};

enum class GeneratorKind {
    None,
    InputSpout,
    InputSyphon,
    InputNdi,
    SolidColor,
    TestPattern,
    InternalFeedback,
};

// Blend / copy modes. Values 0–7 are legacy; 8+ mirror TouchDesigner Composite TOP
// operations (docs.derivative.ca/Composite_TOP) for interoperability.
enum class CopyMode : int {
    Normal = 0,
    Add = 1,
    Multiply = 2,
    Screen = 3,
    Lighten = 4,
    Darken = 5,
    Difference = 6,
    Overlay = 7,

    Atop = 8,
    Average = 9,
    Brightest = 10,
    BurnColor = 11,
    BurnLinear = 12,
    ChromaDifference = 13,
    ColorBlend = 14,
    DarkerColor = 15,
    Dimmest = 16,
    Divide = 17,
    Dodge = 18,
    Exclude = 19,
    Freeze = 20,
    Glow = 21,
    HardLight = 22,
    HardMix = 23,
    Heat = 24,
    HueBlend = 25,
    Inside = 26,
    InsideLuminance = 27,
    Inverse = 28,
    LighterColor = 29,
    LuminanceDifference = 30,
    Maximum = 31,
    Minimum = 32,
    Negate = 33,
    Outside = 34,
    OutsideLuminance = 35,
    Over = 36,
    Pinlight = 37,
    Reflect = 38,
    SoftLight = 39,
    LinearLight = 40,
    StencilLuminance = 41,
    Subtract = 42,
    Subtractive = 43,
    Under = 44,
    VividLight = 45,
    Xor = 46,
    YFilm = 47,
    ZFilm = 48,
    DifferenceVivid = 49,
    DifferenceRgb = 50,
};

enum class MaskType {
    None,
    Rectangle,
    Circle,
    SoftEdge,
    Ellipse,
    Custom,
};

enum class LayerMatteRole : int {
    None = 0,
    LumaMatte = 1,
    AlphaMatte = 2,
    KnockOut = 3,
};

enum class KeyingMode : int {
    Luma = 0,
    Chroma = 1,
};

/// Clip playback behaviour (GrandVJ-style toolbar). Only a subset is enforced by the engine today.
enum class PlayMode : int {
    LoopForward = 0,
    LoopReverse,
    Once,
    PingPong,
    Shuffle,
    TimecodeSync,
    LoopSegment,
    HoldLastFrame,
    PlayBackwardOnce,
    RandomAccess,
    StepFrame,
};

enum class LayerBand : int {
    Back = 0,
    Mid,
    Front,
};

enum class InputType {
    None,
    MidiNote,
    MidiCC,
    /// Polyphonic key pressure (MIDI status 0xA0); `number` is the note.
    MidiAftertouch,
    Key,
    Osc,
};

/// How a `PropertyMapping` reacts to MIDI note (or key) input vs CC faders.
enum class PropertyButtonMode {
    Continuous, ///< CC fader: value scaled with min/max (default for legacy mappings).
    Toggle,     ///< Note: flip boolean or invert normalized value around midpoint.
    SetOnPress, ///< Note: set to `buttonValue` (e.g. enum index, or 0/1 for bool).
};

enum class TriggerTarget {
    Cell,
    BankNext,
    BankPrev,
    BankSelect,
    BankSetSwitch,
    Property,
};

// Small value types

struct MediaItem {
    QUuid id;
    QString path;         // absolute path on disk
    QString displayName;  // optional user-friendly name, falls back to filename
};

struct EffectParam {
    QString name;
    double value = 0.0;
};

struct Effect {
    QString name;
    QList<EffectParam> params;  // up to 4
};

// Ordered post-processing chain for a cell (node editor). Execution order is list order.
struct CellFilterNode {
    QUuid   id = QUuid::createUuid();
    QString typeId; // e.g. blur, color, glow (render pipeline uses this in later milestones)
    QList<EffectParam> params;
};

/// UV border behaviour when coordinates leave [0, 1] (after zoom/rotation).
enum class WrapMode : int {
    Clamp      = 0,
    Repeat     = 1,
    Mirror     = 2,
    MirrorOnce = 3,
    Black      = 4,
};

struct PictureParams {
    double zoom           = 0.0; // -1 (out) .. +1 (in)
    double rotationDeg    = 0.0; // -180 .. +180
    double brightness     = 0.0; // -1 .. +1
    double contrast       = 1.0; // 0 .. 2
    double saturation     = 1.0; // 0 .. 2
    double circularMotion = 0.0; // 0 .. 1 (strength of circular drift)
    WrapMode wrapMode     = WrapMode::Clamp;
};

/// Source image for the feedback live inject.
enum class FeedbackInputMode : int {
    BelowOnly = 0,       // partial mix below F (classic live inject)
    StackComposite = 1,  // below (no key) + above (keyed); unused in classic path
    SceneLoopback = 2,   // previous full mixer frame (legacy)
};

/// How live inject combines with retained history inside the feedback loop
/// (independent of mixer CopyMode used when compositing layer F into the scene).
enum class FeedbackBlendMode : int {
    Add        = 0, // live + hist*retention
    Mix        = 1, // mix(live, gradedHist, retention)
    Screen     = 2,
    Lighten    = 3, // max
    Multiply   = 4,
    Difference = 5,
};

inline constexpr int kFeedbackRingCapacity = 32;
inline constexpr int kFeedbackMaxFrameDelay = kFeedbackRingCapacity - 2;
/// All ring slots are primed when transparency > 0 (full delay pipeline warm-up).
inline constexpr int kFeedbackRingPrimeCount = kFeedbackRingCapacity;
inline constexpr double kFeedbackZoomMin = -0.2;
inline constexpr double kFeedbackZoomMax = 0.2;
inline constexpr double kFeedbackTranslateMin = -0.25;
inline constexpr double kFeedbackTranslateMax = 0.25;

struct FeedbackParams {
    /// Input grade (applied to live inject before filters).
    double inSaturation  = 1.0;   // 0..2
    double inBrightness  = 0.0;   // -1..1
    double inContrast    = 1.0;   // 0..2
    double inHueShift    = 0.0;   // 0..1 hue rotate on inject (MIDI/CC maps 0..1)
    double inGamma       = 1.0;   // 0.1..4
    /// Feedback path grade (applied to transformed history before retention).
    double saturation  = 1.0;   // 0..2
    double brightness  = 0.0;   // -1..1
    double contrast    = 1.0;   // 0..2
    double hueShift    = 0.0;   // 0..1 hue rotate (MIDI/CC maps 0..1)
    double gamma       = 1.0;   // 0.1..4
    double rotationDeg = 0.0;   // 0..360 path rotation (applied on history read)
    double zoom        = 0.0;   // kFeedbackZoomMin..kFeedbackZoomMax (+ = zoom out)
    /// UV pan applied after scale/rotate (pivot 0.5).
    double translateX  = 0.0;   // kFeedbackTranslateMin..Max
    double translateY  = 0.0;   // kFeedbackTranslateMin..Max
    /// History dampening / mix weight depending on blendMode.
    double retention   = 0.95;  // 0..1
    FeedbackBlendMode blendMode = FeedbackBlendMode::Add;
    int frameDelay     = 0;     // 0..kFeedbackMaxFrameDelay (classic path uses ping-pong)
    FeedbackInputMode inputMode = FeedbackInputMode::BelowOnly;
    WrapMode wrapMode = WrapMode::Black; // UV border for history sample
};

struct CellProps {
    int     priority      = 0;
    double  transparency  = 1.0;
    /// Per-clip audio fader (linear, typically 0–2). Video opacity uses `transparency`.
    double  audioGain     = 1.0;
    double  movieSpeed    = 1.0;
    double  fade          = 0.0;
    MaskType maskType     = MaskType::None;
    LayerMatteRole matteRole = LayerMatteRole::None;
    /// Common mask feather / edge softness (0..1).
    double  maskFeather   = 0.1;
    /// Rectangle / soft-edge mask size (0..1, full frame at 1).
    double  maskRectWidth = 1.0;
    double  maskRectHeight = 1.0;
    /// Circle/custom mask radius (0..1).
    double  maskRadius = 0.5;
    /// Ellipse radii (0..1).
    double  maskEllipseX = 0.6;
    double  maskEllipseY = 0.45;
    /// Legacy shared fields (kept for backward compatibility while migrating projects).
    double  maskWidth     = 0.0;
    double  maskSmoothness = 0.0;
    double  rotationZ     = 0.0;
    CopyMode copyMode     = CopyMode::Normal;
    /// Last Mixing preset row chosen in the inspector (0 = Custom).
    int     mixingPresetIndex = 0;
    /// Preferred mixer layer slot (0..12 => UI layers 1..13; GPU layer = +1).
    int preferredLayer = 4;
    /// Key/matte RGB weights (0–1, UI often shows as %). Reserved for GPU keying; defaults 1 = full.
    double  keyChannelR   = 1.0;
    double  keyChannelG   = 1.0;
    double  keyChannelB   = 1.0;
    /// Keying mode used by Mixing tab controls.
    KeyingMode keyingMode = KeyingMode::Luma;
    /// Enables/disables implicit keying from the Mixing tab.
    bool    keyingEnabled = false;
    /// Luma key center (0 = black .. 1 = white).
    double  keyLumaCenter = 0.5;
    /// False = key black/dark, true = key white/bright.
    bool    keyLumaInvert = false;
    /// Key threshold/range for luma/chroma keying.
    double  keyThreshold = 0.25;
    double  keySoftness = 0.12;
    /// Chroma key hue center (0..1 around the color wheel).
    double  keyChromaHue = 0.33;
    /// False = key selected hue, true = invert and keep selected hue.
    bool    keyChromaInvert = false;

    PlayMode playMode        = PlayMode::LoopForward;
    bool     clipPaused      = false;
    /// Segment trim as normalized times in the source (0 = start, 1 = end).
    double   segmentInU      = 0.0;
    double   segmentOutU     = 1.0;
    /// Manual playhead position 0–1 for scratch / jog (applied on seek).
    double   scratchHeadU    = 0.5;
    QString  overlayText;
    QString  tcStart         = QStringLiteral("00:00:00:00");
    PictureParams  picture;
    FeedbackParams feedback;
};

struct VisualRef {
    VisualType   type      = VisualType::Empty;
    QUuid        mediaId;                 // when type == Media
    GeneratorKind generator = GeneratorKind::None;  // when type == Generator
};

struct PropertyMapping {
    QString    property;  // e.g. "transparency", "movieSpeed"
    InputType  input = InputType::None;
    int        channel = 0;
    int        number  = 0;     // CC number / note number
    double     minValue = 0.0;
    double     maxValue = 1.0;
    PropertyButtonMode buttonMode = PropertyButtonMode::Continuous;
    /// Meaningful when `buttonMode == SetOnPress` (e.g. enum index, 1.0 for bool true).
    double     buttonValue = 1.0;
};

struct TriggerMapping {
    InputType     input = InputType::None;
    int           channel = 0;
    int           number  = 0;    // midi number / key code
    QString       keyText;        // for KEY targets, the Qt key-sequence string
    TriggerTarget target = TriggerTarget::Cell;
    int           bankSetIndex = 0;
    int           bankIndex    = 0;
    int           cellIndex    = 0;
    QString       propertyName; // for TriggerTarget::Property
};

struct Cell {
    int         index = 0;  // 0..N inside its bank (depends on grid size)
    QString     name;       // optional user display name (feedback / filter-only cells)
    VisualRef   visual;
    CellProps   props;
    std::optional<Effect> effect;
    QList<PropertyMapping> propertyMappings;
    /// Filter chain stored only on this bank-grid cell (node editor). The mixer applies it
    /// when this cell occupies a mix-layer slot; clip peek and inspector thumbnails stay raw.
    QList<CellFilterNode> filterChain;
};

struct Bank {
    int     index = 0;
    QString name;
    QList<Cell> cells;
};

struct BankSet {
    BankSetType type = BankSetType::TypeA;
    QList<Bank> banks;
};

struct AudioSettings {
    QString driver       = QStringLiteral("auto"); // auto/wasapi/asio/coreaudio/pulse/jack/alsa
    QString inputDevice;
    QString outputDevice;
    int     bufferSize   = 512;
    int     sampleRate   = 48000;
};

struct UiSettings {
    bool mediaLibraryVisible = true;
    bool bankPanelVisible    = true;
    bool parameterTabsVisible = true;
    int  mediaLibraryWidth   = 320;
    int  bankPanelHeight     = 260;
};

struct MatrixSettings {
    int width  = 1920;
    int height = 1080;
    int gridRows = 4;
    int gridCols = 12;
};

/// Project-wide post-mixer NVIDIA filter chain (Output tab only).
struct OutputSettings {
    QList<CellFilterNode> filterChain;
};

struct Settings {
    AudioSettings  audio;
    UiSettings     ui;
    MatrixSettings matrix;
    OutputSettings output;
};

} // namespace pvj::core
