#pragma once

#include <QList>
#include <QString>

namespace pvj::core {

struct CellFilterNode;

enum class FilterEffectFamily {
    Blur,
    Color,
    Transform,
    Distort,
    Kaleido,
    Generate,
    Stylize,
    Key,
    Mask,
    Blend,
    Pattern,
    Utility,
    Light,
    Revival,
    Temporal,
    Film,
    Maxine,
};

enum class FilterExecutionBackend {
    Shader,
    Maxine,
};

struct FilterEffectMeta {
    FilterEffectFamily family = FilterEffectFamily::Utility;
  /// Index within the family shader `switch`.
    int familyId = 0;
  /// Extra draw passes inside one filter node (e.g. separable blur H+V).
    int internalPasses = 1;
  /// W3C blend mode index when family is Blend; -1 otherwise.
    int blendModeOverride = -1;
    FilterExecutionBackend backend = FilterExecutionBackend::Shader;
};

/// True when the typeId has GPU effect metadata (catalog or keying-only).
bool filterEffectIsRegistered(const QString& typeId);

/// Lookup metadata for a catalog typeId (case-insensitive).
FilterEffectMeta filterEffectMeta(const QString& typeId);

/// Internal draw passes for one filter node (e.g. separable blur = 2).
int filterEffectInternalPasses(const QString& typeId);

/// Total catalog entries with registered effect metadata.
int filterEffectCatalogCount();

/// True when the typeId uses the NVIDIA Maxine SDK backend (not GLSL).
bool filterUsesMaxineBackend(const QString& typeId);

/// English category key for Maxine catalog entries.
QString maxineFilterCategoryKey();

/// Reserved structural node type that splits a feedback cell chain into pre/post segments.
QString feedbackMarkerTypeId();

/// True when the typeId is the reserved feedback marker node.
bool isFeedbackMarkerNode(const QString& typeId);

/// Splits a chain at the first feedback marker. If none is present, the whole chain becomes post.
bool splitFilterChainAtFeedbackMarker(const QList<CellFilterNode>& chain,
                                      QList<CellFilterNode>* pre,
                                      QList<CellFilterNode>* post);

/// True when the filter may appear on the Output-tab post-mix chain (NVIDIA category + Maxine backend).
bool isOutputAllowedFilter(const QString& typeId);

/// Keeps only output-allowed nodes; drops invalid entries.
void sanitizeOutputFilterChain(QList<CellFilterNode>& chain);

/// Drops nodes with unknown typeIds; merges params with current schema defaults.
void sanitizeCellFilterChain(QList<CellFilterNode>& chain);

} // namespace pvj::core
