#include <edit_atlas/formats/xlsx/detail/xlsx_workbook_text.hpp>

#include <edit_atlas/core/timeline_projection.hpp>

#include <array>

namespace edit_atlas::formats::xlsx::detail {
namespace {

constexpr auto kEnglishEventColumns = std::to_array<std::string_view>({
    "Event",
    "Reel",
    "Track Type",
    "Track",
    "Edit Type",
    "Transition",
    "Transition Frames",
    "Source In",
    "Source Out",
    "Record In",
    "Record Out",
    "Duration",
    "Duration Frames",
    "Clip Name",
    "Source File",
    "Comments",
    "Source Line",
});

constexpr auto kBrazilianPortugueseEventColumns =
    std::to_array<std::string_view>({
        "Evento",
        "Rolo",
        "Tipo de faixa",
        "Faixa",
        "Tipo de edição",
        "Transição",
        "Quadros da transição",
        "Entrada da fonte",
        "Saída da fonte",
        "Entrada da gravação",
        "Saída da gravação",
        "Duração",
        "Duração em quadros",
        "Nome do clipe",
        "Arquivo de origem",
        "Comentários",
        "Linha de origem",
    });

constexpr auto kEnglishTimelineColumns = std::to_array<std::string_view>({
    "Property",
    "Value",
});

constexpr auto kBrazilianPortugueseTimelineColumns =
    std::to_array<std::string_view>({
        "Propriedade",
        "Valor",
    });

constexpr auto kEnglishDiagnosticColumns = std::to_array<std::string_view>({
    "Severity",
    "Code",
    "Message",
    "Source",
    "Line",
    "Column",
});

constexpr auto kBrazilianPortugueseDiagnosticColumns =
    std::to_array<std::string_view>({
        "Severidade",
        "Código",
        "Mensagem",
        "Origem",
        "Linha",
        "Coluna",
    });

static_assert(kEnglishEventColumns.size() == core::kTimelineEventFieldCount);
static_assert(kBrazilianPortugueseEventColumns.size() ==
              core::kTimelineEventFieldCount);
static_assert(kEnglishTimelineColumns.size() ==
              kBrazilianPortugueseTimelineColumns.size());
static_assert(kEnglishDiagnosticColumns.size() ==
              kBrazilianPortugueseDiagnosticColumns.size());

constexpr WorkbookText kEnglishText{
    .events_sheet = "Events",
    .timeline_sheet = "Timeline",
    .diagnostics_sheet = "Diagnostics",
    .subject = "Editorial timeline report",
    .category = "Editorial",
    .keywords = "EDL, editorial, timeline",
    .comments = "Created by Edit Atlas",
    .event_columns = kEnglishEventColumns,
    .timeline_columns = kEnglishTimelineColumns,
    .diagnostic_columns = kEnglishDiagnosticColumns,
    .title = "Title",
    .frame_rate = "Frame Rate",
    .timecode_mode = "Timecode Mode",
    .drop_frame = "Drop Frame",
    .non_drop_frame = "Non-Drop Frame",
    .event_count = "Event Count",
    .video = "Video",
    .audio = "Audio",
    .data = "Data",
    .other = "Other",
    .cut = "Cut",
    .dissolve = "Dissolve",
    .wipe = "Wipe",
    .key = "Key",
    .info = "Info",
    .warning = "Warning",
    .error = "Error",
};

constexpr WorkbookText kBrazilianPortugueseText{
    .events_sheet = "Eventos",
    .timeline_sheet = "Linha do tempo",
    .diagnostics_sheet = "Diagnósticos",
    .subject = "Relatório de linha do tempo editorial",
    .category = "Editorial",
    .keywords = "EDL, editorial, linha do tempo",
    .comments = "Criado pelo Edit Atlas",
    .event_columns = kBrazilianPortugueseEventColumns,
    .timeline_columns = kBrazilianPortugueseTimelineColumns,
    .diagnostic_columns = kBrazilianPortugueseDiagnosticColumns,
    .title = "Título",
    .frame_rate = "Taxa de quadros",
    .timecode_mode = "Modo de timecode",
    .drop_frame = "Drop Frame",
    .non_drop_frame = "Non-Drop Frame",
    .event_count = "Número de eventos",
    .video = "Vídeo",
    .audio = "Áudio",
    .data = "Dados",
    .other = "Outro",
    .cut = "Corte",
    .dissolve = "Dissolução",
    .wipe = "Transição wipe",
    .key = "Chave",
    .info = "Informação",
    .warning = "Aviso",
    .error = "Erro",
};

} // namespace

const WorkbookText &WorkbookTextFor(WorkbookLanguage language) noexcept {
    switch (language) {
    case WorkbookLanguage::kEnglish:
        return kEnglishText;
    case WorkbookLanguage::kBrazilianPortuguese:
        return kBrazilianPortugueseText;
    }
    return kEnglishText;
}

} // namespace edit_atlas::formats::xlsx::detail
