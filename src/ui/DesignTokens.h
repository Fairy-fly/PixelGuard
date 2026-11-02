#pragma once
#include <utility>

namespace design {
// Shared by the stylesheet resolver, native layouts, and custom painting.
inline constexpr const char* Background = "#F3F4F6";
inline constexpr const char* Surface = "#FAFBFC";
inline constexpr const char* Raised = "#FFFFFF";
inline constexpr const char* Subtle = "#EDEFF3";
inline constexpr const char* Border = "#DCE0E6";
inline constexpr const char* Text = "#232934";
inline constexpr const char* Secondary = "#596374";
inline constexpr const char* Muted = "#788292";
inline constexpr const char* Disabled = "#A1A8B4";
inline constexpr const char* Accent = "#5269AD";
inline constexpr const char* AccentHover = "#435B9C";
inline constexpr const char* AccentPressed = "#374E8D";
inline constexpr const char* AccentSoft = "#E7ECF8";
inline constexpr const char* Difference = "#B46145";
inline constexpr const char* Error = "#B34C4C";
inline constexpr const char* ErrorSoft = "#F9EBE9";
inline constexpr const char* Success = "#377B69";
inline constexpr const char* Canvas = "#252930";
inline constexpr const char* CanvasDeep = "#1D2127";
inline constexpr const char* CanvasLine = "#3A414C";
inline constexpr const char* CanvasText = "#E0E5ED";
inline constexpr const char* CanvasMuted = "#9CA8B9";
inline constexpr const char* ImageEdge = "#515A68";

inline constexpr int Unit = 4;
inline constexpr int Small = 8;
inline constexpr int Gap = 12;
inline constexpr int Margin = 16;
inline constexpr int Large = 24;
inline constexpr int Control = 32;
inline constexpr int Compact = 28;
inline constexpr int Radius = 5;
inline constexpr int BodyFont = 13;
inline constexpr int SmallFont = 12;
inline constexpr int TitleFont = 16;
inline constexpr int MetricFont = 22;
inline constexpr int CanvasInset = 20;

inline constexpr std::pair<const char*, const char*> colors[] = {
    {"Background", Background}, {"Surface", Surface}, {"Raised", Raised}, {"Subtle", Subtle},
    {"Border", Border}, {"Text", Text}, {"Secondary", Secondary}, {"Muted", Muted}, {"Disabled", Disabled},
    {"Accent", Accent}, {"AccentHover", AccentHover}, {"AccentPressed", AccentPressed}, {"AccentSoft", AccentSoft},
    {"Difference", Difference}, {"Error", Error}, {"ErrorSoft", ErrorSoft}, {"Success", Success},
    {"Canvas", Canvas}, {"CanvasDeep", CanvasDeep}, {"CanvasLine", CanvasLine},
    {"CanvasText", CanvasText}, {"CanvasMuted", CanvasMuted}, {"ImageEdge", ImageEdge}
};
}
