#include "rtf.hpp"
#include "ConsoleManager.hpp"
#include "stringUtils.hpp"
#include <filesystem>

bool Rtf::load(const std::wstring& filePath) {
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string fileContent = ss.str();
    file.close();

    return loadFromString(fileContent);
}

std::wstring getIndent(int depth) {
    std::wstring indent = L"";
    for (int i = 0; i < depth; ++i) {
        indent += L"  |";
    }
    return indent + L"-- ";
}

void printRtfBlock(const RtfBlock& block, int depth) {
    if (const auto* paragraph = dynamic_cast<const RtfParagraph*>(&block)) {
        printRtfParagraph(*paragraph, depth);
        return;
    }

    if (const auto* table = dynamic_cast<const RtfTable*>(&block)) {
        printRtfTable(*table, depth);
        return;
    }

    LOG_WARNING(getIndent(depth) + L"Tip RtfBlock necunoscut la adresa: " + std::to_wstring((uintptr_t)&block));
}

void printRtfParagraph(const RtfParagraph& paragraph, int depth) {
    const std::wstring indent = getIndent(depth);
    LOG_INFO(indent + L"PARAGRAPH [Style: Font " + std::to_wstring(paragraph.style.fontSize) + L"pt]");

    for (size_t i = 0; i < paragraph.spans.size(); ++i) {
        const auto& span = paragraph.spans[i];
        std::wstring spanInfo = L"[" + std::to_wstring(i) + L"] ";
        spanInfo += span.text.length() > 30
            ? span.text.substr(0, 27) + L"..."
            : span.text;

        LOG_DEBUG(getIndent(depth + 1) + L"SPAN (Chars: " + std::to_wstring(span.text.length()) + L"): '" + spanInfo + L"'");
    }
}

std::wstring getBorderStyleName(RtfBorderStyle style) {
    switch (style) {
    case RtfBorderStyle::None: return L"None";
    case RtfBorderStyle::Single: return L"Single";
    case RtfBorderStyle::Double: return L"Double";
    default: return L"Unknown";
    }
}

void printBorderSpec(const std::wstring& name, const BorderSpec& spec, int depth) {
    const std::wstring indent = getIndent(depth);
    if (spec.style != RtfBorderStyle::None || spec.widthTwips > 0) {
        LOG_DEBUG(indent + L"-> " + name + L": Style: " + getBorderStyleName(spec.style) +
            L", Width: " + std::to_wstring(spec.widthTwips) + L" twips");
    }
    else {
        LOG_DEBUG(indent + L"-> " + name + L": Not Set (None)");
    }
}

void printRtfCell(const RtfCell& cell, int depth) {
    const std::wstring indent = getIndent(depth);
    LOG_SUCCESS(indent + L"CELL (Colspan: " + std::to_wstring(cell.colspan) +
        L", Rowspan: " + std::to_wstring(cell.rowspan) + L")");

    LOG_INFO(indent + L"  [Padding - Twips]:");
    int paddingDepth = depth + 1;
    const std::wstring paddingIndent = getIndent(paddingDepth);

    LOG_DEBUG(paddingIndent + L"-> Top: " + std::to_wstring(cell.padding.topTwips) + L" twips");
    LOG_DEBUG(paddingIndent + L"-> Bottom: " + std::to_wstring(cell.padding.bottomTwips) + L" twips");
    LOG_DEBUG(paddingIndent + L"-> Left: " + std::to_wstring(cell.padding.leftTwips) + L" twips");
    LOG_DEBUG(paddingIndent + L"-> Right: " + std::to_wstring(cell.padding.rightTwips) + L" twips");

    LOG_INFO(indent + L"  [Borders]:");
    int borderDepth = depth + 1;

    printBorderSpec(L"Left", cell.borders.left, borderDepth);
    printBorderSpec(L"Top", cell.borders.top, borderDepth);
    printBorderSpec(L"Right", cell.borders.right, borderDepth);
    printBorderSpec(L"Bottom", cell.borders.bottom, borderDepth);

    LOG_INFO(indent + L"  [Content Blocks]:");
    for (const auto& block : cell.content) {
        if (block) {
            printRtfBlock(*block, depth + 1);
        }
        else {
            LOG_ERROR(getIndent(depth + 1) + L"Eroare: RtfBlock NULL în celulă.");
        }
    }
}

void printRtfTable(const RtfTable& table, int depth) {
    const std::wstring indent = getIndent(depth);
    LOG_WARNING(indent + L"TABLE (Rows: " + std::to_wstring(table.rows.size()) + L")");

    for (size_t r = 0; r < table.rows.size(); ++r) {
        const auto& row = table.rows[r];
        LOG_WARNING(getIndent(depth + 1) + L"ROW [" + std::to_wstring(r) + L"] (Cells: " + std::to_wstring(row.cells.size()) + L")");

        for (size_t c = 0; c < row.cells.size(); ++c) {
            const auto& cell = row.cells[c];
            printRtfCell(cell, depth + 2);
        }
    }
}

void Rtf::print() const {
    LOG(L"\n=======================================================");
    LOG(L"| DUMP STRUCTURĂ DOCUMENT RTF ÎNCĂRCAT (Rtf::print()) |");
    LOG(L"=======================================================");

    LOG_INFO(L"--- HEADER BLOCKS (" + std::to_wstring(headerBlocks.size()) + L") ---");
    for (size_t i = 0; i < headerBlocks.size(); ++i) {
        if (headerBlocks[i]) {
            LOG_INFO(L"Header Bloc [" + std::to_wstring(i) + L"]:");
            printRtfBlock(*headerBlocks[i], 1);
        }
    }

    LOG_INFO(L"--- FOOTER BLOCKS (" + std::to_wstring(footerBlocks.size()) + L") ---");
    for (size_t i = 0; i < footerBlocks.size(); ++i) {
        if (footerBlocks[i]) {
            LOG_INFO(L"Footer Bloc [" + std::to_wstring(i) + L"]:");
            printRtfBlock(*footerBlocks[i], 1);
        }
    }

    LOG_INFO(L"--- BODY BLOCKS (" + std::to_wstring(blocks.size()) + L") ---");
    if (blocks.empty()) {
        LOG_INFO(L"Documentul RTF nu conține blocuri principale.");
        return;
    }

    for (size_t i = 0; i < blocks.size(); ++i) {
        const auto& block = blocks[i];
        if (block) {
            LOG_INFO(L"Bloc de nivel 0 [" + std::to_wstring(i) + L"]:");
            printRtfBlock(*block, 1);
        }
        else {
            LOG_FATAL(L"Eroare FATALĂ: Bloc de nivel 0 [" + std::to_wstring(i) + L"] este NULL!");
        }
    }

    LOG(L"=======================================================");
    LOG_SUCCESS(L"Dump-ul structurii a fost finalizat cu succes.");
}

std::vector<std::unique_ptr<RtfBlock>> Rtf::parseRtfContent(const std::string& content) {
    std::vector<std::unique_ptr<RtfBlock>> parsedBlocks;

    if (pageConfigurations.empty()) {
        RtfPage defaultPage;
        pageConfigurations.push_back(defaultPage);
    }

    RtfPage& activePageConfig = pageConfigurations.back();
    RtfParseState state(activePageConfig);

    state.currentStyle.fontSize = 12.0;
    state.currentParagraph = std::make_unique<RtfParagraph>();

    for (size_t i = 0; i < content.length(); ++i) {
        char c = content[i];
        unsigned int codePage = (state.ansicpg == 0) ? 1250 : state.ansicpg;

        if (c == '{') {
            state.styleStack.push_back(state.currentStyle);
            state.metadataStack.push_back(state.isParsingMetadata);
            state.footerStack.push_back(state.isParsingFooter);
            state.headerStack.push_back(state.isParsingHeader);
        }
        else if (c == '}') {
            if (state.parsingFontTable) {
                if (!state.currentTextBuffer.empty() && state.currentFontIndexForTable != -1) {
                    std::wstring fontName = state.currentTextBuffer;
                    if (!fontName.empty() && fontName.back() == L';') {
                        fontName.pop_back();
                    }
                    state.fontTable[state.currentFontIndexForTable] = fontName;
                }
                state.parsingFontTable = false;
                state.currentFontIndexForTable = -1;
                state.currentTextBuffer.clear();
            }

            auto block = finalizeCurrentParagraph(state, parsedBlocks);
            if (block) {
                parsedBlocks.push_back(std::move(block));
            }

            // ⭐ SALVARE TABEL DOAR CÂND IEȘIM DIN SCOPE-UL DE HEADER SAU FOOTER
            if (!state.footerStack.empty()) {
                bool wasFooter = state.isParsingFooter;
                state.isParsingFooter = state.footerStack.back();
                state.footerStack.pop_back();

                if (wasFooter && !state.isParsingFooter && state.currentTable && !state.currentTable->rows.empty()) {
                    this->footerBlocks.push_back(std::move(state.currentTable));
                    state.currentTable = nullptr;
                    state.inTable = false;
                }
            }
            if (!state.headerStack.empty()) {
                bool wasHeader = state.isParsingHeader;
                state.isParsingHeader = state.headerStack.back();
                state.headerStack.pop_back();

                if (wasHeader && !state.isParsingHeader && state.currentTable && !state.currentTable->rows.empty()) {
                    this->headerBlocks.push_back(std::move(state.currentTable));
                    state.currentTable = nullptr;
                    state.inTable = false;
                }
            }
            if (!state.metadataStack.empty()) {
                state.isParsingMetadata = state.metadataStack.back();
                state.metadataStack.pop_back();
            }
            if (!state.styleStack.empty()) {
                state.currentStyle = state.styleStack.back();
                state.styleStack.pop_back();
            }
        }
        else if (c == '\\') {
            i++;
            if (i >= content.length()) break;

            char next_char = content[i];

            if (next_char == '{' || next_char == '}' || next_char == '\\' || next_char == '-') {
                std::string singleCharStr(&next_char, 1);
                std::wstring wideChar = convertSingleByteToWideChar(singleCharStr, codePage);
                state.currentTextBuffer += wideChar;
                continue;
            }
            else if (next_char == '~') {
                state.currentTextBuffer += L"\u00A0";
                continue;
            }
            else if (next_char == '\'') {
                if (i + 2 < content.length()) {
                    std::string hexStr = content.substr(i + 1, 2);
                    i += 2;

                    char byteVal = static_cast<char>(std::strtol(hexStr.c_str(), nullptr, 16));
                    std::string singleCharStr(1, byteVal);

                    unsigned int codePage = (state.ansicpg == 0) ? 1252 : state.ansicpg;
                    std::wstring wideChar = convertSingleByteToWideChar(singleCharStr, codePage);
                    state.currentTextBuffer += wideChar;
                }
                continue;
            }

            if (isalpha(next_char)) {
                std::string controlWordStr;

                while (i < content.length() && isalpha(content[i])) {
                    controlWordStr += content[i];
                    i++;
                }

                int parameter = 0;
                bool hasParam = false;

                if (i < content.length() && (isdigit(content[i]) || content[i] == '-')) {
                    hasParam = true;
                    int sign = 1;
                    if (content[i] == '-') { sign = -1; i++; }
                    while (i < content.length() && isdigit(content[i])) {
                        parameter = parameter * 10 + (content[i] - '0');
                        i++;
                    }
                    parameter *= sign;
                }

                if (!hasParam) {
                    parameter = 1;
                }

                if (i < content.length() && content[i] == ' ') {
                    i++;
                }

                std::wstring controlWord(controlWordStr.begin(), controlWordStr.end());

                if (controlWord == L"fonttbl" || controlWord == L"colortbl" || controlWord == L"stylesheet") {
                    state.isParsingMetadata = true;
                    LOG_DEBUG(L"Rtf::parseRtfContent: ACTIVAT flag-ul de metadate pentru: " + controlWord);
                }
                handleControlWord(state, controlWord, parameter, parsedBlocks);

                i--;
            }
        }
        else {
            if (state.parsingFontTable || state.isParsingMetadata) {
                continue;
            }

            if (state.skipCharsRemaining > 0) {
                state.skipCharsRemaining--;
                continue;
            }

            if (state.currentParagraph == nullptr) {
                state.currentParagraph = std::make_unique<RtfParagraph>();
            }

            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                if (state.currentTextBuffer.empty() && state.currentParagraph && state.currentParagraph->spans.empty()) {
                    continue;
                }
                if (c == '\n' || c == '\r' || c == '\t') {
                    c = ' ';
                }
            }

            std::string singleCharStr(&c, 1);
            std::wstring wideChar = convertSingleByteToWideChar(singleCharStr, codePage);

            state.currentTextBuffer += wideChar;
        }

    } // Sfârșitul buclei for

    auto block = finalizeCurrentParagraph(state, parsedBlocks);
    if (block) {
        parsedBlocks.push_back(std::move(block));
    }

    if (state.currentTable && !state.currentTable->rows.empty()) {
        if (state.isParsingFooter) {
            this->footerBlocks.push_back(std::move(state.currentTable));
        }
        else if (state.isParsingHeader) {
            this->headerBlocks.push_back(std::move(state.currentTable));
        }
        else {
            parsedBlocks.push_back(std::move(state.currentTable));
        }
        state.inTable = false;
    }

    return parsedBlocks;
}

std::unique_ptr<RtfBlock> Rtf::finalizeCurrentParagraph(RtfParseState& state, std::vector<std::unique_ptr<RtfBlock>>& parsedBlocks) {
    if (state.isParsingMetadata) {
        state.currentTextBuffer.clear();
        state.currentParagraph = std::make_unique<RtfParagraph>();
        return nullptr;
    }

    finalizeCurrentSpan(state);

    if (state.currentParagraph == nullptr) {
        state.currentParagraph = std::make_unique<RtfParagraph>();
        return nullptr;
    }

    bool is_empty = state.currentParagraph->spans.empty();
    if (!is_empty) {
        bool hasRealText = false;
        for (const auto& span : state.currentParagraph->spans) {
            if (span.text.find_first_not_of(L" \n\r\t") != std::wstring::npos) {
                hasRealText = true;
                break;
            }
        }
        if (!hasRealText) {
            is_empty = true;
        }
    }

    // 1. ÎN INTERIORUL UNUI TABEL (state.inTable == true)
    if (state.inTable) {
        if (state.currentCell && !is_empty) {
            state.currentParagraph->style = state.currentStyle;
            state.currentCell->content.push_back(std::move(state.currentParagraph));
        }

        state.currentParagraph = std::make_unique<RtfParagraph>();
        return nullptr;
    }

    // 2. ÎN AFARA TABELULUI (!state.inTable)
    // Dacă exista un tabel activ neînchis, îl finalizăm și îl salvăm o singură dată
    if (state.currentTable && !state.currentTable->rows.empty()) {
        if (state.isParsingFooter) {
            this->footerBlocks.push_back(std::move(state.currentTable));
        }
        else if (state.isParsingHeader) {
            this->headerBlocks.push_back(std::move(state.currentTable));
        }
        else {
            parsedBlocks.push_back(std::move(state.currentTable));
        }
        state.currentTable = nullptr;
    }

    if (is_empty) {
        state.currentParagraph->spans.clear();
        RtfSpan emptySpan;
        emptySpan.text = L" ";
        emptySpan.style = state.currentStyle;
        state.currentParagraph->spans.push_back(emptySpan);
    }

    state.currentParagraph->style = state.currentStyle;

    std::unique_ptr<RtfBlock> block = std::move(state.currentParagraph);
    state.currentParagraph = std::make_unique<RtfParagraph>();

    if (state.isParsingFooter) {
        this->footerBlocks.push_back(std::move(block));
        return nullptr;
    }
    else if (state.isParsingHeader) {
        this->headerBlocks.push_back(std::move(block));
        return nullptr;
    }

    return block;
}

void Rtf::handleControlWord(RtfParseState& state, const std::wstring& word, int param,
    std::vector<std::unique_ptr<RtfBlock>>& parsedBlocks) {

    // --- 1. STIL ȘI FORMAT TEXT ---
    if (word == L"fs") {
        finalizeCurrentSpan(state);
        if (!state.currentParagraph) {
            state.currentParagraph = std::make_unique<RtfParagraph>();
        }
        state.currentStyle.fontSize = param / 2.0;
    }
    else if (word == L"b") {
        finalizeCurrentSpan(state);
        if (!state.currentParagraph) {
            state.currentParagraph = std::make_unique<RtfParagraph>();
        }

        state.currentStyle.fontWeight = (param != 0) ? L"bold" : L"normal";

        if (state.currentParagraph) {
            state.currentParagraph->style.fontWeight = state.currentStyle.fontWeight;
        }
    }
    else if (word == L"i") {
        finalizeCurrentSpan(state);
        if (!state.currentParagraph) {
            state.currentParagraph = std::make_unique<RtfParagraph>();
        }
        state.currentStyle.fontStyle = (param != 0) ? L"italic" : L"normal";
    }
    else if (word == L"ul") {
        finalizeCurrentSpan(state);
        if (!state.currentParagraph) {
            state.currentParagraph = std::make_unique<RtfParagraph>();
        }
        state.currentStyle.textDecoration = (param != 0) ? L"underline" : L"none";
    }

    // --- 2. PAGINĂ ȘI MARGINI ---
    else if (word == L"landscape" || word == L"lndscpsxn") {
        LOG_DEBUG(L"Setează orientarea: Landscape");
        state.pageConfig.setOrientation(RtfOrientation::Landscape);
    }
    else if (word == L"paperw") {
        LOG_DEBUG(L"Setează lățimea paginii: " + std::to_wstring(param) + L" twips");
        state.pageConfig.setWidthTwips(param);
    }
    else if (word == L"paperh") {
        LOG_DEBUG(L"Setează înălțimea paginii: " + std::to_wstring(param) + L" twips");
        state.pageConfig.setHeightTwips(param);
    }
    else if (word == L"margl") {
        LOG_DEBUG(L"Setează marginea stângă: " + std::to_wstring(param) + L" twips");
        state.pageConfig.setMargins(
            state.pageConfig.getMarginTopTwips(),
            state.pageConfig.getMarginRightTwips(),
            state.pageConfig.getMarginBottomTwips(),
            param
        );
    }
    else if (word == L"margr") {
        LOG_DEBUG(L"Setează marginea dreaptă: " + std::to_wstring(param) + L" twips");
        state.pageConfig.setMargins(
            state.pageConfig.getMarginTopTwips(),
            param,
            state.pageConfig.getMarginBottomTwips(),
            state.pageConfig.getMarginLeftTwips()
        );
    }
    else if (word == L"margt") {
        LOG_DEBUG(L"Setează marginea de sus: " + std::to_wstring(param) + L" twips");
        state.pageConfig.setMargins(
            param,
            state.pageConfig.getMarginRightTwips(),
            state.pageConfig.getMarginBottomTwips(),
            state.pageConfig.getMarginLeftTwips()
        );
    }
    else if (word == L"margb") {
        LOG_DEBUG(L"Setează marginea de jos: " + std::to_wstring(param) + L" twips");
        state.pageConfig.setMargins(
            state.pageConfig.getMarginTopTwips(),
            state.pageConfig.getMarginRightTwips(),
            param,
            state.pageConfig.getMarginLeftTwips()
        );
    }

    // --- 3. BORDURI DE TABEL ---
    else if (word.rfind(L"clbrdr", 0) == 0) {
        if (word == L"clbrdrl") {
            state.borderLeftPending = true;
        }
        else if (word == L"clbrdrt") {
            state.borderTopPending = true;
        }
        else if (word == L"clbrdrb") {
            state.borderBottomPending = true;
        }
        else if (word == L"clbrdrr") {
            state.borderRightPending = true;
        }
    }
    else if (word == L"brdrw") {
        state.currentBorderSpec.widthTwips = param;
        applyPendingBorders(state);
    }
    else if (word == L"brdrs") {
        state.currentBorderSpec.style = RtfBorderStyle::Single;
        applyPendingBorders(state);
    }
    else if (word == L"brdrdb") {
        state.currentBorderSpec.style = RtfBorderStyle::Double;
        applyPendingBorders(state);
    }

    // --- 4. PADDING CELULE ---
    else if (word == L"clpadb") {
        if (param > 0) {
            state.currentCellPadding.bottomTwips = param;
        }
    }
    else if (word == L"clpadt") {
        if (param > 0) {
            state.currentCellPadding.topTwips = param;
        }
    }
    else if (word == L"clpadl") {
        if (param > 0) {
            state.currentCellPadding.leftTwips = param;
        }
    }
    else if (word == L"clpadr") {
        if (param > 0) {
            state.currentCellPadding.rightTwips = param;
        }
    }

    // --- 5. PARAGRAF, STRUCTURĂ ȘI ALINIERE ---
    else if (word == L"par" || word == L"pard") {
        if (state.inTable && state.currentCell != nullptr) {
            finalizeCurrentParagraph(state, parsedBlocks);
        }
        else {
            // Când întâlnim un \par în afara unei celule de tabel, marcăm ieșirea din tabel
            state.inTable = false;
            auto block = finalizeCurrentParagraph(state, parsedBlocks);
            if (block) {
                parsedBlocks.push_back(std::move(block));
            }
        }

        state.currentParagraph = std::make_unique<RtfParagraph>();

        if (word == L"pard") {
            state.currentStyle.textAlign = L"left";
            state.currentStyle.fontWeight = L"normal";
            state.currentStyle.fontStyle = L"normal";
        }
        state.currentParagraph->style = state.currentStyle;
    }
    else if (word == L"line") {
        finalizeCurrentSpan(state);
        if (!state.currentParagraph) {
            state.currentParagraph = std::make_unique<RtfParagraph>();
        }
        state.currentTextBuffer += L"$line$";
        Rtf::finalizeCurrentSpan(state);
    }
    else if (word == L"tab") {
        if (state.isParsingMetadata) return;

        finalizeCurrentSpan(state);
        state.currentTextBuffer += L"\\t";
        finalizeCurrentSpan(state);
    }
    else if (word == L"page") {
        if (state.currentParagraph) {
            LOG_WARNING(L"ADAUGAREA HARD PAGE BREAK (\\page)");
            state.currentTextBuffer += L"\f";
            Rtf::finalizeCurrentSpan(state);
        }
    }
    else if (word == L"footer") {
        auto block = finalizeCurrentParagraph(state, parsedBlocks);
        if (block) {
            parsedBlocks.push_back(std::move(block));
        }
        if (state.currentTable && !state.currentTable->rows.empty()) {
            parsedBlocks.push_back(std::move(state.currentTable));
            state.currentTable = nullptr;
            state.inTable = false;
        }
        this->footerBlocks.clear();
        state.isParsingFooter = true;
        state.isParsingHeader = false;
        LOG_SUCCESS(L"INTRU IN FOOTER");
    }
    else if (word == L"header") {
        auto block = finalizeCurrentParagraph(state, parsedBlocks);
        if (block) {
            parsedBlocks.push_back(std::move(block));
        }
        if (state.currentTable && !state.currentTable->rows.empty()) {
            parsedBlocks.push_back(std::move(state.currentTable));
            state.currentTable = nullptr;
            state.inTable = false;
        }
        this->headerBlocks.clear();
        state.isParsingHeader = true;
        state.isParsingFooter = false;
        LOG_SUCCESS(L"INTRU IN HEADER");
    }
    else if (word == L"qc") {
        finalizeCurrentSpan(state);
        if (!state.currentParagraph) {
            state.currentParagraph = std::make_unique<RtfParagraph>();
        }
        state.currentStyle.textAlign = L"center";
        state.currentParagraph->style = state.currentStyle;
    }
    else if (word == L"qr") {
        finalizeCurrentSpan(state);
        if (!state.currentParagraph) {
            state.currentParagraph = std::make_unique<RtfParagraph>();
        }
        state.currentStyle.textAlign = L"right";
        state.currentParagraph->style = state.currentStyle;
    }
    else if (word == L"ql") {
        finalizeCurrentSpan(state);
        if (!state.currentParagraph) {
            state.currentParagraph = std::make_unique<RtfParagraph>();
        }
        state.currentStyle.textAlign = L"left";
        state.currentParagraph->style = state.currentStyle;
    }
    else if (word == L"qj") {
        finalizeCurrentSpan(state);
        if (!state.currentParagraph) {
            state.currentParagraph = std::make_unique<RtfParagraph>();
        }
        state.currentStyle.textAlign = L"justify";
        state.currentParagraph->style = state.currentStyle;
    }

    // --- 6. CONSTRUCȚIE TABELE ---
    else if (word == L"trhdr") {
        state.currentRow.isHeader = true;
        LOG_DEBUG(L"Rând marcat ca HEADER de tabel (\\trhdr)");
    }
    else if (word == L"trowd") {
        state.currentTextBuffer.clear();

        // Păstrăm tabelul existent pentru a concatena rândurile consecutive!
        if (!state.currentTable) {
            state.currentTable = std::make_unique<RtfTable>();
        }

        state.inTable = true;
        state.currentRow = RtfRow{};
        state.currentCellIndex = 0;
        state.currentCell = nullptr;
    }
    else if (word == L"intbl") {
        if (!state.inTable) return;

        if (state.currentCell == nullptr && state.currentCellIndex >= 0 &&
            state.currentCellIndex < static_cast<int>(state.currentRow.cells.size()))
        {
            state.currentCell = &state.currentRow.cells[state.currentCellIndex];
        }

        finalizeCurrentParagraph(state, parsedBlocks);
        state.currentParagraph = std::make_unique<RtfParagraph>();
        state.currentParagraph->style = state.currentStyle;
    }
    else if (word.rfind(L"cellx", 0) == 0) {
        if (!state.inTable) return;

        int twips = param;
        if (twips <= 0) return;

        if (state.currentCell == nullptr) {
            state.currentRow.cells.emplace_back();
            state.currentCell = &state.currentRow.cells.back();
        }

        state.currentCell->borders = state.currentCellBorders;
        state.currentCellBorders = CellBorders();

        state.currentCell->padding = state.currentCellPadding;
        state.currentCellPadding = {};

        double widthPt = static_cast<double>(twips) / 20.0;
        state.currentRow.columnWidthsPt.push_back(widthPt);

        if (state.currentTable && state.currentTable->rows.empty()) {
            state.currentTable->columnWidthsPt.push_back(widthPt);
        }

        state.currentCell = nullptr;
    }
    else if (word == L"cell") {
        if (!state.inTable) return;

        finalizeCurrentParagraph(state, parsedBlocks);
        state.currentCell = nullptr;
        state.currentCellIndex++;
        state.currentCellBorders = CellBorders();
    }
    else if (word == L"row") {
        if (!state.inTable || !state.currentTable) return;

        finalizeCurrentParagraph(state, parsedBlocks);

        if (state.currentCell && state.currentCell->content.empty() && !state.currentRow.cells.empty()) {
            state.currentRow.cells.pop_back();
        }

        if (state.currentTable->columnWidthsPt.empty()) {
            state.currentTable->columnWidthsPt = state.currentRow.columnWidthsPt;
        }

        state.currentTable->rows.push_back(std::move(state.currentRow));

        state.currentRow = RtfRow{};
        state.currentCell = nullptr;
        state.currentCellIndex = -1;

        state.currentParagraph = std::make_unique<RtfParagraph>();
    }

    // --- 7. METADATE, FONTURI ȘI CAMPUURI ---
    else if (word == L"fonttbl") {
        state.isParsingMetadata = true;
        state.parsingFontTable = true;
        LOG_DEBUG(L"Rtf::parseRtfContent: ACTIVAT flag-ul de metadate pentru: fonttbl");
    }
    else if (word == L"ansicpg") {
        state.ansicpg = param;
        LOG_DEBUG(L"Setează code page: " + std::to_wstring(param));
    }
    else if (word == L"f" && param != -1) {
        long fontIndex = param;

        if (state.parsingFontTable) {
            state.currentFontIndexForTable = fontIndex;
            LOG_DEBUG(L"FONT TABLE: Index font curent stocat: " + std::to_wstring(fontIndex));
        }
        else {
            if (state.fontTable.count(fontIndex)) {
                std::wstring fontName = state.fontTable.at(fontIndex);

                finalizeCurrentSpan(state);
                state.currentStyle.fontFamily = fontName;

                if (state.currentParagraph) {
                    state.currentParagraph->style.fontFamily = fontName;
                }

                LOG_DEBUG(L"DEBUG FONT: Font setat (Aplicare) la: " + fontName + L" (index: " + std::to_wstring(fontIndex) + L")");
            }
            else {
                LOG_WARNING(L"WARNING FONT: Index font " + std::to_wstring(fontIndex) + L" nu este definit. Nu se aplică fontul.");
            }
        }
    }
    else if (word == L"chpgn") {
        finalizeCurrentSpan(state);
        state.currentTextBuffer += L"\\chpgn";
        finalizeCurrentSpan(state);
        return;
    }
    else if (word == L"numpages" || word == L"nofpages") {
        state.currentTextBuffer += L"\\numpages";
    }
    else if (word == L"sa") {
        finalizeCurrentSpan(state);
        state.currentStyle.spaceAfterPt = static_cast<float>(param) / 20.0f;
        if (state.currentParagraph) {
            state.currentParagraph->style.spaceAfterPt = state.currentStyle.spaceAfterPt;
        }
    }

    // --- 8. DIRECTIVE GLOBALE / IGNORATE TĂCUT ---
    else if (word == L"rtf" || word == L"ansi" || word == L"deff" ||
        word == L"trgaph" || word == L"sb" ||
        word == L"sl" || word == L"slmult" || word == L"widowctrl" ||
        word == L"ftnbj" || word == L"aenddoc" || word == L"formshade" ||
        word == L"viewkind" || word == L"uc" || word == L"cf") {
        LOG_DEBUG(L"Rtf::handleControlWord: Comandă document/ignorată: " + word);
    }
    else if (word == L"uc") {
        state.ucCount = param;
    }
    else if (word == L"u") {
        wchar_t unicodeChar = static_cast<wchar_t>(param);
        state.currentTextBuffer += unicodeChar;
        state.skipCharsRemaining = state.ucCount;
    }
    else {
        LOG_DEBUG(L"Rtf::handleControlWord: cuvânt de control necunoscut: " + word);
    }
}

void Rtf::finalizeCurrentSpan(RtfParseState& state) {
    if (state.currentParagraph && !state.currentTextBuffer.empty()) {
        RtfSpan newSpan;
        newSpan.text = state.currentTextBuffer;
        newSpan.style = state.currentStyle;
        state.currentParagraph->spans.push_back(std::move(newSpan));
        state.currentTextBuffer.clear();
    }
}

void Rtf::applyPendingBorders(RtfParseState& state) {
    if (state.currentBorderSpec.isSet()) {
        if (state.borderLeftPending) {
            state.currentCellBorders.left = state.currentBorderSpec;
            state.borderLeftPending = false;
        }
        if (state.borderTopPending) {
            state.currentCellBorders.top = state.currentBorderSpec;
            state.borderTopPending = false;
        }
        if (state.borderBottomPending) {
            state.currentCellBorders.bottom = state.currentBorderSpec;
            state.borderBottomPending = false;
        }
        if (state.borderRightPending) {
            state.currentCellBorders.right = state.currentBorderSpec;
            state.borderRightPending = false;
        }
    }
}