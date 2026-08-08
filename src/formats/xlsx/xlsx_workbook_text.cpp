#include <edit_atlas/formats/xlsx/detail/xlsx_workbook_text.hpp>

#include <array>
#include <cstddef>

namespace edit_atlas::formats::xlsx::detail {
namespace {

struct LocalizedText final {
    std::string_view english;
    std::string_view brazilian_portuguese;

    [[nodiscard]] std::string_view
    For(WorkbookLanguage language) const noexcept {
        switch (language) {
        case WorkbookLanguage::kEnglish:
            return english;
        case WorkbookLanguage::kBrazilianPortuguese:
            return brazilian_portuguese;
        }
        return english;
    }
};

// Entries are indexed by WorkbookTextKey. Keep this order synchronized with
// the enum declaration in xlsx_workbook_text.hpp.
constexpr std::array kTexts{
    LocalizedText{"Events", "Eventos"},
    LocalizedText{"Timeline", "Linha do tempo"},
    LocalizedText{"Diagnostics", "Diagnósticos"},
    LocalizedText{"Editorial timeline report",
                  "Relatório de linha do tempo editorial"},
    LocalizedText{"Editorial", "Editorial"},
    LocalizedText{"EDL, editorial, timeline", "EDL, editorial, linha do tempo"},
    LocalizedText{"Created by Edit Atlas", "Criado pelo Edit Atlas"},
    LocalizedText{"Title", "Título"},
    LocalizedText{"Frame Rate", "Taxa de quadros"},
    LocalizedText{"Timecode Mode", "Modo de timecode"},
    LocalizedText{"Drop Frame", "Drop Frame"},
    LocalizedText{"Non-Drop Frame", "Non-Drop Frame"},
    LocalizedText{"Event Count", "Número de eventos"},
    LocalizedText{"Video", "Vídeo"},
    LocalizedText{"Audio", "Áudio"},
    LocalizedText{"Data", "Dados"},
    LocalizedText{"Other", "Outro"},
    LocalizedText{"Cut", "Corte"},
    LocalizedText{"Dissolve", "Dissolução"},
    LocalizedText{"Wipe", "Transição wipe"},
    LocalizedText{"Key", "Chave"},
    LocalizedText{"Info", "Informação"},
    LocalizedText{"Warning", "Aviso"},
    LocalizedText{"Error", "Erro"},
    LocalizedText{"Property", "Propriedade"},
    LocalizedText{"Value", "Valor"},
    LocalizedText{"Severity", "Severidade"},
    LocalizedText{"Code", "Código"},
    LocalizedText{"Message", "Mensagem"},
    LocalizedText{"Source", "Origem"},
    LocalizedText{"Line", "Linha"},
    LocalizedText{"Column", "Coluna"},
};

// Entries are indexed by TimelineEventField. Keep this order synchronized
// with the enum declaration in timeline_projection.hpp.
constexpr std::array kEventColumns{
    LocalizedText{"Event", "Evento"},
    LocalizedText{"Reel", "Rolo"},
    LocalizedText{"Track Type", "Tipo de faixa"},
    LocalizedText{"Track", "Faixa"},
    LocalizedText{"Edit Type", "Tipo de edição"},
    LocalizedText{"Transition", "Transição"},
    LocalizedText{"Transition Frames", "Quadros da transição"},
    LocalizedText{"Source In", "Entrada da fonte"},
    LocalizedText{"Source Out", "Saída da fonte"},
    LocalizedText{"Record In", "Entrada da gravação"},
    LocalizedText{"Record Out", "Saída da gravação"},
    LocalizedText{"Duration", "Duração"},
    LocalizedText{"Duration Frames", "Duração em quadros"},
    LocalizedText{"Clip Name", "Nome do clipe"},
    LocalizedText{"Source File", "Arquivo de origem"},
    LocalizedText{"Comments", "Comentários"},
    LocalizedText{"Source Line", "Linha de origem"},
};

constexpr std::array kTimelineColumns{
    WorkbookTextKey::kTimelinePropertyColumn,
    WorkbookTextKey::kTimelineValueColumn,
};

constexpr std::array kDiagnosticColumns{
    WorkbookTextKey::kDiagnosticSeverityColumn,
    WorkbookTextKey::kDiagnosticCodeColumn,
    WorkbookTextKey::kDiagnosticMessageColumn,
    WorkbookTextKey::kDiagnosticSourceColumn,
    WorkbookTextKey::kDiagnosticLineColumn,
    WorkbookTextKey::kDiagnosticColumnColumn,
};

static_assert(kTexts.size() ==
              static_cast<std::size_t>(WorkbookTextKey::kCount));
static_assert(kEventColumns.size() == core::kTimelineEventFieldCount);

} // namespace

WorkbookText::WorkbookText(WorkbookLanguage language) noexcept
    : language_(language) {}

std::string_view WorkbookText::Get(WorkbookTextKey key) const noexcept {
    const auto index = static_cast<std::size_t>(key);
    if (index >= kTexts.size()) {
        return {};
    }
    return kTexts[index].For(language_);
}

std::string_view
WorkbookText::EventColumn(core::TimelineEventField field) const noexcept {
    const auto index = static_cast<std::size_t>(field);
    if (index >= kEventColumns.size()) {
        return {};
    }
    return kEventColumns[index].For(language_);
}

std::span<const WorkbookTextKey>
WorkbookText::TimelineColumns(void) const noexcept {
    return kTimelineColumns;
}

std::span<const WorkbookTextKey>
WorkbookText::DiagnosticColumns(void) const noexcept {
    return kDiagnosticColumns;
}

const WorkbookText &WorkbookTextFor(WorkbookLanguage language) noexcept {
    static const WorkbookText kEnglish{WorkbookLanguage::kEnglish};
    static const WorkbookText kBrazilianPortuguese{
        WorkbookLanguage::kBrazilianPortuguese};
    switch (language) {
    case WorkbookLanguage::kEnglish:
        return kEnglish;
    case WorkbookLanguage::kBrazilianPortuguese:
        return kBrazilianPortuguese;
    }
    return kEnglish;
}

} // namespace edit_atlas::formats::xlsx::detail
