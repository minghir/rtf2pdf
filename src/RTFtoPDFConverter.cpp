#include "RTFtoPDFConverter.hpp"
#include "ConvertUtils.hpp" // Adăugăm utilitarul pentru twipsToPoints


#include <algorithm>
#include <cmath> 
#include <functional>

// --- Helper Functions (Definițiile trebuie să fie disponibile) ---

extern std::string wstring_to_utf8(const std::wstring& wstr);
extern std::wstring str_to_wstr(const std::string& str);


double calculateRowHeight(const RtfRow& row, const std::vector<double>& colWidths) {
    double maxHeight = 15.0; // Înălțime minimă de siguranță

    for (const auto& cell : row.cells) {
        double cellHeight = 0.0;
        for (const auto& block : cell.content) {
            if (const auto* para = dynamic_cast<const RtfParagraph*>(block.get())) {
                double fontSize = para->style.fontSize > 0 ? para->style.fontSize : 10.0;
                double lineHeight = fontSize * (para->style.lineHeight > 0 ? para->style.lineHeight : 1.2);
                cellHeight += lineHeight;
            }
        }
        // Adăugăm padding-ul celulei (conversie din twips în pt)
        cellHeight += (cell.padding.topTwips + cell.padding.bottomTwips) / 20.0;
        if (cellHeight > maxHeight) maxHeight = cellHeight;
    }
    return maxHeight;
}


static std::vector<ParagraphLine> buildParagraphLines(
    const RtfParagraph& paragraph,
    const std::function<std::wstring(const std::wstring&)>& textReplacer);

bool RtfToPdfConverter::prepareAndRunPipeline() {
    // 1. Extrage informațiile paginii (în Twips)
    const RtfPage& pageInfoTwips = m_rtfDocument.getPageInfo();

    // ⭐ CONVERSIE: Calculează dimensiunile și marginile în PUNCTE PDF
    pageWidth = ConvertUtils::twipsToPoints(pageInfoTwips.getWidthTwips());
    pageHeight = ConvertUtils::twipsToPoints(pageInfoTwips.getHeightTwips());

    m_marginLeft = ConvertUtils::twipsToPoints(pageInfoTwips.getMarginLeftTwips());
    m_marginRight = ConvertUtils::twipsToPoints(pageInfoTwips.getMarginRightTwips());
    double marginTop = ConvertUtils::twipsToPoints(pageInfoTwips.getMarginTopTwips());

    // Fallback anti-crash pentru dimensiuni de pagină invalide/zero
    if (pageWidth <= 0.0) pageWidth = 595.28;  // Standard A4 portrait (points)
    if (pageHeight <= 0.0) pageHeight = 841.89;

    // Inițializare state
    m_pageHeight = pageHeight;
    m_contentWidth = pageWidth - m_marginLeft - m_marginRight;
    if (m_contentWidth <= 0.0) {
        m_contentWidth = pageWidth - 72.0; // Margin minimă de siguranță de câte 36pt (0.5 inch)
        m_marginLeft = 36.0;
        m_marginRight = 36.0;
    }
    m_currentY = marginTop;

    // Curățăm coada de randare din eventualele rulări anterioare
    m_renderQueue.clear();

    RenderInstruction instruction;
    instruction.renderFunction = L"startPage";
    instruction.width = pageWidth;
    instruction.height = pageHeight;
    m_renderQueue.push_back(instruction);

    // Pornirea primei pagini
    m_currentPageNumber = 1;
    m_totalPagesCount = m_currentPageNumber;
    
    renderHeader();

    LOG_SUCCESS(L"Incepe conversia RTF. Dimensiune continut: " + std::to_wstring(m_contentWidth) + L"pt.");

    // 2. Traversarea documentului (Body)
    for (const auto& block : m_rtfDocument.getBlocks()) {
        if (block) { // Verifica pointer valid
            processRtfBlock(*block);
        }
    }
    renderFooter();

    finalizePageNumbers();

    initializeGlobalVarResolvers();

    if (finalizeAndPaint()) {
        LOG_SUCCESS(L"Conversie finalizată cu succes. Total pagini: " + std::to_wstring(m_currentPageNumber));
        return true;
    }
    else {
        LOG_ERROR(L"Eroare la conversia in pdf!");
        return false;
    }
}

// -----------------------------------------------------------------------------
// 1. Metoda pentru Disk (Salvare pe HDD)
// -----------------------------------------------------------------------------
bool RtfToPdfConverter::convert(const std::wstring& filename) {
    outputFilePath = filename;

    // Pregătim dimensiunile paginii din RTF
    const RtfPage& pageInfo = m_rtfDocument.getPageInfo();
    double w = ConvertUtils::twipsToPoints(pageInfo.getWidthTwips());
    double h = ConvertUtils::twipsToPoints(pageInfo.getHeightTwips());

    if (w <= 0.0) w = 595.28;
    if (h <= 0.0) h = 841.89;

    // ⭐ 1. Inițializăm O SINGURĂ DATĂ pe disc
    if (!m_pdfWriter.initialize(outputFilePath, w, h)) {
        LOG_FATAL(L"Eroare fatală la inițializarea PdfWriter. Nu se poate continua.");
        return false;
    }

    // ⭐ 2. Rulăm pipeline-ul de randare (FĂRĂ re-inițializarea din memorie!)
    if (!prepareAndRunPipeline()) {
        return false;
    }

    // ⭐ 3. Finalizăm fișierul pe disc (Scrie trailer-ul și închide fișierul corect!)
    if (!m_pdfWriter.finalize()) {
        LOG_ERROR(L"[RTF2PDF] Apelul m_pdfWriter.finalize() a eșuat la salvarea pe disc!");
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// 2. METODA PENTRU MEMORIE (RAM)
// -----------------------------------------------------------------------------
bool RtfToPdfConverter::convertToMemory(std::vector<uint8_t>& outPdfBuffer) {
    outPdfBuffer.clear();

    const RtfPage& pageInfo = m_rtfDocument.getPageInfo();
    double w = ConvertUtils::twipsToPoints(pageInfo.getWidthTwips());
    double h = ConvertUtils::twipsToPoints(pageInfo.getHeightTwips());

    if (w <= 0.0) w = 595.28;
    if (h <= 0.0) h = 841.89;

    // ⭐ 1. Inițializăm în memorie O SINGURĂ DATĂ AICI
    if (!m_pdfWriter.initializeToMemory(w, h)) {
        LOG_ERROR(L"[RTF2PDF] Eșec critic la inițializarea m_pdfWriter în memorie!");
        return false;
    }

    // ⭐ 2. Rulăm pipeline-ul de preparare și randare
    if (!prepareAndRunPipeline()) {
        LOG_ERROR(L"[RTF2PDF] Eșec la executarea pipeline-ului de randare!");
        return false;
    }

    // ⭐ 3. Finalizăm documentul PDF
    if (!m_pdfWriter.finalize()) {
        LOG_ERROR(L"[RTF2PDF] Apelul m_pdfWriter.finalize() a returnat false!");
        return false;
    }

    // ⭐ 4. Extragerea datelor din stream
    std::string pdfBinaryData = m_pdfWriter.getMemoryData();

    if (pdfBinaryData.empty()) {
        LOG_ERROR(L"[RTF2PDF] Buffer-ul PDF din m_pdfWriter este gol!");
        return false;
    }

    outPdfBuffer.assign(pdfBinaryData.begin(), pdfBinaryData.end());

    LOG_SUCCESS(L"[RTF2PDF] Conversia în memorie a reușit! Dimensiune PDF: " +
        std::to_wstring(outPdfBuffer.size()) + L" octeți.");
    return true;
}

/*
void RtfToPdfConverter::newLineAndCheckPageBreak(double requiredHeight) {
    const RtfPage& pageInfoTwips = m_rtfDocument.getPageInfo();
    const double EPSILON = 0.1;

    // Marginile de Jos și de Sus în Puncte
    double marginBottom = ConvertUtils::twipsToPoints(pageInfoTwips.getMarginBottomTwips());
    double marginTop = ConvertUtils::twipsToPoints(pageInfoTwips.getMarginTopTwips());

    // ⭐ CORECȚIE CRITICĂ: Scădem și înălțimea subsolului (getFooterHeight())
    // astfel încât corpul să se oprească ÎNAINTE de zona rezervată subsolului.
    double footerHeight = getFooterHeight();
    double y_remaining_usable_space = m_pageHeight - m_currentY - marginBottom - footerHeight;

    // Dacă spațiul rămas e mai mic strict decât înălțimea necesară
    if (requiredHeight > y_remaining_usable_space + EPSILON) {
        LOG_INFO(L"[PAGINARE] Pagina plină. Trecere la pagina următoare.");

        // 1. Randare Footer pe pagina curentă
        renderFooter();

        // 2. Finalizează pagina curentă
        RenderInstruction instruction;
        instruction.renderFunction = L"endPage";
        m_renderQueue.push_back(instruction);

        // 3. Deschide o pagină nouă 
        pageWidth = ConvertUtils::twipsToPoints(pageInfoTwips.getWidthTwips());
        pageHeight = ConvertUtils::twipsToPoints(pageInfoTwips.getHeightTwips());

        instruction.renderFunction = L"startPage";
        instruction.width = pageWidth;
        instruction.height = pageHeight;
        m_renderQueue.push_back(instruction);

        m_currentPageNumber++;
        m_totalPagesCount++;

        // 4. Resetează cursorul Y la marginea de sus
        m_currentY = marginTop;

        // 5. Randare Header pe noua pagină
        renderHeader();
    }
}
*/
void RtfToPdfConverter::newLineAndCheckPageBreak(double requiredHeight) {
    const RtfPage& pageInfoTwips = m_rtfDocument.getPageInfo();
    const double EPSILON = 0.1;

    double marginBottom = ConvertUtils::twipsToPoints(pageInfoTwips.getMarginBottomTwips());
    double marginTop = ConvertUtils::twipsToPoints(pageInfoTwips.getMarginTopTwips());
    double footerHeight = getFooterHeight();
    double y_remaining_usable_space = m_pageHeight - m_currentY - marginBottom - footerHeight;

    if (requiredHeight > y_remaining_usable_space + EPSILON) {
        LOG_INFO(L"[PAGINARE] Pagina plină. Trecere la pagina următoare.");

        renderFooter();

        RenderInstruction instruction;
        instruction.renderFunction = L"endPage";
        m_renderQueue.push_back(instruction);

        pageWidth = ConvertUtils::twipsToPoints(pageInfoTwips.getWidthTwips());
        pageHeight = ConvertUtils::twipsToPoints(pageInfoTwips.getHeightTwips());

        instruction.renderFunction = L"startPage";
        instruction.width = pageWidth;
        instruction.height = pageHeight;
        m_renderQueue.push_back(instruction);

        m_currentPageNumber++;
        m_totalPagesCount++;

        m_currentY = marginTop;

        renderHeader();

        // Re-randare cap de tabel pe pagina nouă
        if (m_isProcessingTable && !m_isRenderingHeader && !m_currentTableHeaderRows.empty()) {
            m_isRenderingHeader = true;
            for (const auto* hRow : m_currentTableHeaderRows) {
                processRtfRow(*hRow, hRow->columnWidthsPt); // ⭐ Lățimile specifice fiecărui rând din antet
            }
            m_isRenderingHeader = false;
        }
    }
}

// -----------------------------------------------------------------
// 2. FUNCȚII DE RANDARE STRUCTURALĂ
// -----------------------------------------------------------------

/**
 * Traversează un element block (paragraf, tabel, div, etc.).
 */
void RtfToPdfConverter::processRtfBlock(const RtfBlock& block) {

    // ⭐ Notă: RtfBlock este clasa de bază. Trebuie să accesăm Style-ul din obiectele derivate.
    //double marginTop = 5.0; // Placeholder
    //double marginBottom = 5.0; // Placeholder



    // 2. Procesează tipul specific de bloc
    if (const RtfParagraph* paragraph = dynamic_cast<const RtfParagraph*>(&block)) {

       

        processRtfParagraph(*paragraph);

        double marginTop = 2.0; // Placeholder
        m_currentY += marginTop;

    }
    else if (const RtfTable* table = dynamic_cast<const RtfTable*>(&block)) {

       // double marginTop = 2.0; // Placeholder
       //m_currentY += marginTop;
        processRtfTable(*table); // TODO: Implementare Tabel
    }
    // ... alte tipuri de block (liste etc.)

   // m_currentY += marginBottom;
}

// ----------------------------------------------------
// NOU: Funcție Auxiliară pentru Calculul Poziției X de Start a unui Rând
// ----------------------------------------------------

double RtfToPdfConverter::calculateLineXStart(const std::wstring& alignment, double contentWidth, double lineWidth, double marginLeft) const {
    if (alignment == L"center") {
        // Aliniere Centru: marginLeft + (availableSpace / 2)
        // availableSpace = contentWidth - lineWidth
        double availableSpace = contentWidth - lineWidth;
        return marginLeft + (availableSpace / 2.0);
    }
    else if (alignment == L"right") {
        // Aliniere Dreapta: marginLeft + contentWidth - lineWidth
        return marginLeft + contentWidth - lineWidth;
    }
    else {
        // Aliniere Stânga (Implicit): marginLeft
        return marginLeft;
    }
}

// ----------------------------------------------------
// Funcția Principală Modificată: processRtfParagraph
// ----------------------------------------------------

// ----------------------------------------------------
// NOUA Funcție processRtfParagraph
// ----------------------------------------------------

double RtfToPdfConverter::processRtfParagraph(const RtfParagraph& paragraph) {

    double startY = m_currentY; // Salvează poziția Y de start
    const Style& paragraphStyle = paragraph.style;

    std::wstring fontFamily = paragraphStyle.fontFamily;
    if (fontFamily.empty() || fontFamily == L"DefaultFont") {
        fontFamily = L"Arial"; // Folosește Arial ca fallback
    }


    double fontSize = paragraphStyle.fontSize > 0.0 ? paragraphStyle.fontSize : 12.0;

    if (fontSize == 0.0 || fontSize > 20.0) { // Presupunem că > 20.0 pt e o eroare de parsare default (24 half-points)
        fontSize = 12.0; // Fallback la 12pt
    }

    double lineHeightFactor = paragraphStyle.lineHeight > 0.0 ? paragraphStyle.lineHeight : 1.2;

    // Calculul CORECT: înmulțește fontul cu factorul
    double lineHeight = fontSize * lineHeightFactor;

    //LOG_ERROR(L"INALTIME LINEIE CALCULATA:" + std::to_wstring(lineHeight) + L" Si font:" + std::to_wstring(fontSize));
    // 1. Avansare inițială Y și verificare paginare
    //m_currentY += lineHeight;
   // newLineAndCheckPageBreak(lineHeight);

    // 2. Poziția X de start pentru conținut
    double content_x_start = m_marginLeft;// +paragraphStyle.boxModel.paddingLeft;
    double current_x = content_x_start;
    //double y_baseline = m_pageHeight - m_currentY;

    

    // Buffer pentru a acumula lățimea textului pe rândul CURENT (pentru centrare/wrap)
    //std::vector<const RtfSpan*> spansOnCurrentLine;

    // Buffer pentru a acumula textul și stilurile pe rândul CURENT
    std::vector<std::pair<std::wstring, Style>> wordsOnCurrentLine;
    double currentLineContentWidth = 0.0; // Lățimea totală a span-urilor pe rândul curent

    // 3. Iterează și Randează fiecare SPAN
    for (const auto& span : paragraph.spans) {
        const Style& spanStyle = span.style;
        std::vector<std::wstring> words = split_to_words(span.text);
        //print_wstr_vct(words);
        // Iterează prin fiecare CUVÂNT din SPAN
        for (const std::wstring& word : words) {

            // ⭐ A. TRATAREA \page (Hard Page Break) ⭐
            if (word == L"\f") {
                LOG_SUCCESS(L"HARD PAGE BREAK DETECTAT (\\page)");

                // 1. Randare Rând Rămas: Randează orice conținut acumulat înainte de \page
                if (!wordsOnCurrentLine.empty()) {
                    renderWords(wordsOnCurrentLine, paragraphStyle.textAlign, currentLineContentWidth, lineHeight);
                }

                // 2. FORȚAREA SCHIMBĂRII DE PAGINĂ
                // Apelăm newLineAndCheckPageBreak cu o înălțime mare (sau o funcție dedicată)
                // pentru a forța schimbul de pagină.
                // Folosim o valoare suficient de mare pentru a forța întotdeauna un page break.
                newLineAndCheckPageBreak(m_pageHeight);

                // 3. Resetează variabilele (pe noua pagină)
                current_x = content_x_start;
                currentLineContentWidth = 0.0;
                wordsOnCurrentLine.clear();

                continue;
            }


            // ⭐ A. TRATAREA \line (Hard Break) ⭐
            
            //if (word == L"\\n") {
            if (word == L"$line$") {

                // Pasul 1: Randare Rând CURENT (dacă nu e gol)
                if (!wordsOnCurrentLine.empty()) {
                    // Aceasta avansează Y (în interiorul renderWords)
                    //LOG_ERROR(L"ADAUG LINIE NOUA LA PARAGRAF");
                    renderWords(wordsOnCurrentLine, paragraphStyle.textAlign, currentLineContentWidth, lineHeight);
                    //newLineAndCheckPageBreak(lineHeight); // Verifică și schimbă pagina
                    //m_currentY += lineHeight; // Avansare Y #1 (Pentru rândul randat)
                }
                else {
                    // Pasul 1b (NOU): Dacă linia este goală, forțează avansarea Y
                    // Această logică DUPLICĂ avansarea Y din renderWords, dar este crucială aici.
                    //LOG_ERROR(L"ADAUG LINIE NOUA");
                    newLineAndCheckPageBreak(lineHeight);
                    m_currentY += lineHeight;
                }

                // Pasul 2: Resetează variabilele pentru noul rând
                current_x = content_x_start;
                currentLineContentWidth = 0.0;
                wordsOnCurrentLine.clear();

                // Treci la următorul token
                //continue;
            }
            //if (word.length() == 1 && word[0] == L'\t') {
            if (word == L"\\t") {

                // Măsurăm lățimea tabulatorului (36.0 pt)
                double tabWidth = TAB_WIDTH;

                // 🎯 LOGICA DE WORD WRAP pentru TAB
                // Verifică dacă tabulatorul + tot ce e pe rând depășește lățimea conținutului.
                // Tab-ul nu rupe rândul, dar dacă nu încape, rândul anterior trebuie randat.
                if (current_x + tabWidth > m_marginLeft + m_contentWidth) {

                    // Randează conținutul curent și resetează
                    if (!wordsOnCurrentLine.empty()) {
                        renderWords(wordsOnCurrentLine, paragraphStyle.textAlign, currentLineContentWidth, lineHeight);
                    }
                    current_x = content_x_start;
                    currentLineContentWidth = 0.0;
                    wordsOnCurrentLine.clear();
                }

                // 1. Adaugă tab-ul ca element în buffer-ul liniei
                wordsOnCurrentLine.push_back({ L"\\t", spanStyle });

                // 2. Actualizează lățimea liniei
                currentLineContentWidth += tabWidth;
                current_x += tabWidth;

                // Nu mai avem nevoie de `continue`, deoarece `\t` este tratat
                // ca orice alt cuvânt, dar este adăugat la buffer.
                // continue; // Ștergeți acest rând
                continue;
            }
            
            // Măsoară lățimea cuvântului + spațiul (un spațiu după fiecare cuvânt)
            double wordWidth = m_pdfWriter.measureTextWidth(word + L" ", spanStyle);
            // 🎯 LOGICA DE WORD WRAP
            // Verifică dacă cuvântul curent încape pe rândul curent
            
            if (current_x + wordWidth > m_marginLeft + m_contentWidth) {
            
                // A. Finalizează și Randează Rândul CURENT (dacă nu e gol)
                if (!wordsOnCurrentLine.empty()) {
                    renderWords(wordsOnCurrentLine, paragraphStyle.textAlign, currentLineContentWidth, lineHeight);
                }
                // B. Resetează variabilele pentru noul rând
                current_x = content_x_start; // Începe de la stânga
               // m_currentY += lineHeight;
                currentLineContentWidth = 0.0;
                wordsOnCurrentLine.clear();
            }
            // 3. Adaugă Cuvântul CURENT la linia nouă (sau la cea existentă)
            wordsOnCurrentLine.push_back({ word + L" ", spanStyle });
            currentLineContentWidth += wordWidth;
            current_x += wordWidth; // Avansăm cursorul X

        }
    }

    // 4. Randare Rând Rămas (Ultimul rând)
    //if (!wordsOnCurrentLine.empty()) {
    //    renderWords(wordsOnCurrentLine, paragraphStyle.textAlign, currentLineContentWidth, lineHeight);
    //}

    if (!wordsOnCurrentLine.empty()) {
        // Dacă alinierea este justify, forțează alinierea la stânga pentru ultimul rând
        std::wstring finalAlignment = (paragraphStyle.textAlign == L"justify") ? L"left" : paragraphStyle.textAlign;

        renderWords(wordsOnCurrentLine, finalAlignment, currentLineContentWidth, lineHeight);
    }

    double endY = m_currentY;

    LOG_DEBUG(L"Paragraf procesat. Cursor Y mentinut la: " + std::to_wstring(m_currentY));

    return endY - startY; // Returnează înălțimea totală consumată
    
}



void RtfToPdfConverter::renderWords(
    const std::vector<std::pair<std::wstring, Style>>& words,
    const std::wstring& alignment,
    double lineContentWidth,
    double lineHeight)
{
    // Trecere la Rând Nou și Verificare Paginare
    newLineAndCheckPageBreak(lineHeight);
    m_currentY += lineHeight;

    double line_width = m_contentWidth; // Lățimea totală de conținut disponibilă
    double currentX = m_marginLeft;
    double yBaseline = m_pageHeight - m_currentY;

    // Variabile specifice Justify
    bool isJustify = (alignment == L"justify" && words.size() > 1);
    double extraSpacePerGap = 0.0;

    // 1. CALCULUL SPAȚIULUI EXTRA PENTRU JUSTIFY
    if (isJustify) {
        size_t numSpacesToExpand = words.size() - 1;

        if (numSpacesToExpand > 0) {
            // lineContentWidth include lățimea tuturor cuvintelor + lățimea spațiilor originale
            double remainingSpace = line_width - lineContentWidth;

            // Spațiul suplimentar care trebuie adăugat fiecărui spațiu dintre cuvinte
            extraSpacePerGap = remainingSpace / numSpacesToExpand;

            currentX = m_marginLeft; // Pentru Justify, randarea începe întotdeauna de la margine
        }
        else {
            // Dacă există un singur cuvânt, dezactivează Justify și aliniază stânga
            isJustify = false;
        }
    }

    // 2. CALCULUL X START PENTRU NON-JUSTIFY
    if (!isJustify) {
        currentX = calculateLineXStart(
            alignment,
            line_width,
            lineContentWidth,
            m_marginLeft
        );
    }

    // 3. RANDAREA PROPRIU-ZISĂ
    for (size_t i = 0; i < words.size(); ++i) {
        const std::wstring& wordWithSpace = words[i].first;
        const Style& style = words[i].second;
        
        if (wordWithSpace == L"$line$" ||  wordWithSpace == L"$line$ " || wordWithSpace == L" $line$" || wordWithSpace == L" $line$ ") continue;

        if (wordWithSpace == L"\\t") {
            // Nu randăm nimic, doar avansăm cursorul.
            // Presupunând că tabWidth era 36.0 (sau lățimea tab-ului)
            //LOG_WARNING(L"AM GASIT TAAAAAAAAAAAAB");
            currentX += TAB_WIDTH;
            continue; // Treci la următorul cuvânt/spațiu
        }
       
        // Separă cuvântul de spațiul de la coadă (asumând un spațiu)
        std::wstring wordOnly;
        if (wordWithSpace.length() > 0 && wordWithSpace.back() == L' ') {
            wordOnly = wordWithSpace.substr(0, wordWithSpace.length() - 1);
        }
        else {
            // Caz de siguranță/eroare: randează tot șirul dacă nu se termină cu spațiu.
            wordOnly = wordWithSpace;
        }
        //std::wstring wordOnly = wordWithSpace.substr(0, wordWithSpace.length() - 1);

        // Calculează lățimea doar a cuvântului
        double wordOnlyWidth = m_pdfWriter.measureTextWidth(wordOnly, style);

        // --- Generează Instrucțiunea de Randare (DOAR CUVÂNTUL) ---
        RenderInstruction instruction;
        instruction.x = currentX;
        instruction.y = yBaseline;
        instruction.text_content = wstr_trim(wordOnly); // Textul final randat este DOAR cuvântul
        instruction.style = style;
        instruction.renderFunction = L"text";
        // Apelați identifyGlobalVars aici dacă este necesar!
        // identifyGlobalVars(wordOnly, instruction.globalVars); 

        m_renderQueue.push_back(instruction);

        // Avansăm cursorul X cu lățimea cuvântului
        currentX += wordOnlyWidth;

        // 4. Avansarea cu spațiul (Spațiul Original + Spațiu Extra)
        if (i < words.size() - 1) { // Aplică spațiu doar între cuvinte

            // Lățimea spațiului original (L" ")
            double originalSpaceWidth = m_pdfWriter.measureTextWidth(L" ", style);

            double totalSpaceAdvance = originalSpaceWidth;

            if (isJustify) {
                // Adaugă spațiul suplimentar (extraSpacePerGap)
                totalSpaceAdvance += extraSpacePerGap;
            }

            currentX += totalSpaceAdvance; // Avansăm cu spațiul ajustat
        }
    }
}


// ----------------------------------------------------
// NOU: Funcție Auxiliară pentru Randarea Span-urilor
// ----------------------------------------------------

void RtfToPdfConverter::renderSpans(const std::vector<const RtfSpan*>& spans, double startX, double yBaseline) {
    double currentX = startX;

    for (const auto* span : spans) {
        // Folosește direct addTextWithSyle cu stilul corect al SPAN-ului
        RenderInstruction instruction;
        instruction.x = currentX;
        instruction.y = yBaseline;
        instruction.text_content = span->text;
        instruction.style = span->style;
        instruction.renderFunction = L"text";
        m_renderQueue.push_back(instruction);

        //pdfWriter.addTextWithSyle(currentX, yBaseline, span->text, span->style);

        // Măsoară lățimea pentru a avansa cursorul
        double spanWidth = m_pdfWriter.measureTextWidth(span->text, span->style);
        currentX += spanWidth;
    }
}
// -----------------------------------------------------------------
// 3. FUNCȚII DE RANDARE PRIMITIVE (Apelează Wrapper-ul)
// -----------------------------------------------------------------


/*
void RtfToPdfConverter::processRtfTable(const RtfTable& table) {
    LOG_WARNING(L"Randare tabel RTF: Incepe. Randuri: " + std::to_wstring(table.rows.size()));

    if (table.rows.empty()) {
        LOG_WARNING(L"Tabelul este gol, randare anulată.");
        return;
    }

    // 1. Colectăm rândurile marcate ca antet (\trhdr) de la începutul tabelului
    std::vector<const RtfRow*> headerRows;
    for (const auto& row : table.rows) {
        if (row.isHeader) {
            headerRows.push_back(&row);
        }
        else {
            break; // Antetele sunt întotdeauna consecutive la începutul tabelului
        }
    }

    // 2. Procesăm rând cu rând
    for (size_t i = 0; i < table.rows.size(); ++i) {
        const RtfRow& row = table.rows[i];

        // Estimăm înălțimea rândului curent (ex: 15pt de siguranță)
        double estimatedRowHeight = 15.0;

        // Salvăm numărul paginii curente înainte de verificarea de paginare
        int pageBefore = m_currentPageNumber;

        // Verificăm dacă rândul curent încape pe pagină sau forțăm pagina nouă
        newLineAndCheckPageBreak(estimatedRowHeight);

        // ⭐ DACA S-A TRECUT PE O PAGINĂ NOUĂ:
        // Re-randăm capul de tabel înainte de a randa rândul de date curent
        if (m_currentPageNumber > pageBefore && !row.isHeader && !headerRows.empty()) {
            LOG_DEBUG(L"Tabelul continuă pe o pagină nouă -> Re-randare cap de tabel.");
            for (const auto* hRow : headerRows) {
                processRtfRow(*hRow, table.columnWidthsPt);
            }
        }

        // 3. Randăm rândul curent
        processRtfRow(row, table.columnWidthsPt);
    }
}
*/
void RtfToPdfConverter::processRtfTable(const RtfTable& table) {
    if (table.rows.empty()) return;

    // 1. Colectăm rândurile marcate ca antet (\trhdr)
    std::vector<const RtfRow*> headerRows;
    for (const auto& row : table.rows) {
        if (row.isHeader) {
            headerRows.push_back(&row);
        }
        else {
            break;
        }
    }

    m_isProcessingTable = true;
    m_currentTableHeaderRows = headerRows;

    // 2. Randăm fiecare rând cu propriile sale lățimi de coloană
    for (size_t i = 0; i < table.rows.size(); ++i) {
        const auto& row = table.rows[i];
        processRtfRow(row, row.columnWidthsPt); // ⭐ Trimitem row.columnWidthsPt
    }

    m_isProcessingTable = false;
    m_currentTableHeaderRows.clear();
}


std::wstring paragraphToText(const RtfParagraph& paragraph) {
    std::wstring result;
    for (const auto& span : paragraph.spans) {
        result += span.text;   // concatenăm textul fiecărui span
    }
    return result;
}



std::wstring extractCellText(const RtfCell& cell) {
    std::wstring result;
    for (const auto& block : cell.content) {
        if (const RtfParagraph* paragraph = dynamic_cast<const RtfParagraph*>(block.get())) {
            // presupunem că RtfParagraph are o metodă getText()
            result += paragraphToText(*paragraph);

           // result += L"\n"; // separăm paragrafele
        }
        // aici poți trata și alte tipuri de block (liste, imagini etc.)
    }
    return result;
}



/*
void RtfToPdfConverter::processRtfRow(const RtfRow& row, const std::vector<double>& colWidths) {

    if (row.cells.empty() || colWidths.empty()) {
        LOG_WARNING(L"Rand rând tabel: celule sau lățimi de coloană lipsă.");
        return;
    }

    // Variabilă care urmărește înălțimea maximă reală de care are nevoie rândul.
    double maxRowHeight = 0.0;

    // Lista înălțimilor reale folosite de celule, pentru a avansa m_currentY corect.
    std::vector<double> cellHeightsUsed;

    // ⭐ Marginea stângă a documentului (offset-ul tabelului pe pagină)
    double tableMarginLeftPt = m_marginLeft;

    // Variabila care urmărește limita stângă ABSOLUTĂ a celulei curente pe pagină.
    double cellStartBoundaryPt = tableMarginLeftPt;

    // Toate celulele din rând încep la aceeași înălțime Y
    double cellStartTopY = m_currentY;

    if (row.cells.size() != colWidths.size())
        LOG_ERROR(L"Lipsesc dimensiuni: Numar celule != Numar latimi");

    // ---------------------------------------------------------------------
    // 1. RANDAREA CONȚINUTULUI ȘI MĂSURAREA ÎNĂLȚIMII REALE
    // ---------------------------------------------------------------------

    for (size_t j = 0; j < row.cells.size() && j < colWidths.size(); ++j) {

        // Limita ABSOLUTĂ RTF (de la 0)
        double rtfEndBoundaryPt = colWidths[j];

        // Granița ABSOLUTĂ DE SFÂRȘIT pe pagină
        double cellEndBoundaryPt = tableMarginLeftPt + rtfEndBoundaryPt;

        // Lățimea Celulei
        double cellWidth = cellEndBoundaryPt - cellStartBoundaryPt;

        // Poziția de start a randării celulei
        double cellStartX = cellStartBoundaryPt;

        if (cellWidth <= 0.0) {
            LOG_ERROR(L"Lățime celulă non-pozitivă detectată. Sărire celulă.");
            cellStartBoundaryPt = cellEndBoundaryPt;
            continue;
        }

        // ⭐ Randarea propriu-zisă a conținutului celulei
        // processRtfCell() va reseta m_currentY la cellStartTopY la final.
        double actualHeight = processRtfCell(row.cells[j], cellStartX, cellStartTopY, cellWidth, 0.0);

        cellHeightsUsed.push_back(actualHeight);
        // ⭐ Actualizăm înălțimea maximă reală
        maxRowHeight = std::max<double>(maxRowHeight, actualHeight);

        // Actualizăm limita ABSOLUTĂ de start pentru următoarea celulă.
        cellStartBoundaryPt = cellEndBoundaryPt;
    }

    // ---------------------------------------------------------------------
    // 2. VERIFICARE PAGINARE FINALĂ ȘI AVANSARE (UNICA) PE Y
    // ---------------------------------------------------------------------

    // Verificare Paginare: Asigură-te că rândul încape în pagina curentă
    // (Această verificare ar trebui să folosească înălțimea reală calculată)
    // ⚠️ NOTĂ: Dacă se face schimb de pagină aici, bordurile ar trebui randate
    // pe pagina nouă, dar pentru simplitate, presupunem că rămânem pe aceeași pagină.
    newLineAndCheckPageBreak(maxRowHeight);

    // ---------------------------------------------------------------------
    // 3. DESENAREA BORDURILOR CELULELOR (Folosind înălțimea MAXIMĂ)
    // ---------------------------------------------------------------------

    double currentCellStartX = tableMarginLeftPt; // Resetăm X-ul pentru desenare

    // Coordonatele Y în sistemul PDF (0 la bază)
    double yBottomPdf = m_pageHeight - cellStartTopY - maxRowHeight; // Partea de jos a rândului
    double yTopPdf = m_pageHeight - cellStartTopY;                    // Partea de sus a rândului

    for (size_t j = 0; j < row.cells.size() && j < colWidths.size(); ++j) {
        const auto& cell = row.cells[j];

        double rtfEndBoundaryPt = colWidths[j];
        double cellEndBoundaryPt = tableMarginLeftPt + rtfEndBoundaryPt;
        double cellWidth = cellEndBoundaryPt - currentCellStartX;

        if (cellWidth > 0.0) {
            double xLeft = currentCellStartX;
            double xRight = cellEndBoundaryPt;
         //   LOG_ERROR(L"Celula " + std::to_wstring(j) + L" - Borduri setate:");
         //   LOG_ERROR(L"  Left set: " + std::to_wstring(cell.borders.left.isSet()) + L", Width: " + std::to_wstring(cell.borders.left.widthTwips));
         //   LOG_ERROR(L"  Top set: " + std::to_wstring(cell.borders.top.isSet()) + L", Width: " + std::to_wstring(cell.borders.top.widthTwips));
         //   LOG_ERROR(L"  Right set: " + std::to_wstring(cell.borders.right.isSet()) + L", Width: " + std::to_wstring(cell.borders.right.widthTwips));
         //   LOG_ERROR(L"  Bottom set: " + std::to_wstring(cell.borders.bottom.isSet()) + L", Width: " + std::to_wstring(cell.borders.bottom.widthTwips));
            // ... și pentru Bottom/Right.
            // Bordura Stânga (\clbrdrl)
            renderCellBorder(cell.borders.left, xLeft, yTopPdf, xLeft, yBottomPdf);

            // Bordura Dreapta (\clbrdrr)
            renderCellBorder(cell.borders.right, xRight, yTopPdf, xRight, yBottomPdf);

            // Bordura Sus (\clbrdrt)
            renderCellBorder(cell.borders.top, xLeft, yTopPdf, xRight, yTopPdf);

            // Bordura Jos (\clbrdrb)
            renderCellBorder(cell.borders.bottom, xLeft, yBottomPdf, xRight, yBottomPdf);
        }

        currentCellStartX = cellEndBoundaryPt;
    }

    // ---------------------------------------------------------------------
    // 4. AVANSAREA FINALĂ A CURSORULUI Y
    // ---------------------------------------------------------------------

    // Avansarea cursorului Y O SINGURĂ DATĂ, cu înălțimea maximă reală
    m_currentY += maxRowHeight;

    //LOG_DEBUG(L"Randare rând tabel RTF finalizată. Înălțime rând: " + std::to_wstring(maxRowHeight) + L"pt. Cursor Y avansat.");
}
*/
/*
void RtfToPdfConverter::processRtfRow(const RtfRow& row, const std::vector<double>& colWidths) {
    if (row.cells.empty() || colWidths.empty()) return;

    double tableMarginLeftPt = m_marginLeft;
    double cellStartTopY = m_currentY;

    // 1. Verificare paginare preventivă
    double estimatedRowHeight = 15.0;
    newLineAndCheckPageBreak(estimatedRowHeight);
    cellStartTopY = m_currentY;

    // 2. Randare celule și calcul înălțime rând
    double maxRowHeight = 0.0;

    for (size_t j = 0; j < row.cells.size(); ++j) {
        const RtfCell& cell = row.cells[j];

        // ⭐ Dacă celula este continuarea unei comasări (\clmrg), o sărim
        if (cell.isMergeNext) continue;

        double cellStartX = (j == 0) ? tableMarginLeftPt : (tableMarginLeftPt + colWidths[j - 1]);

        // Calculăm poziția finală luând în calcul colspan-ul
        size_t endColIdx = std::min<size_t>(j + cell.colspan - 1, colWidths.size() - 1);
        double cellEndX = tableMarginLeftPt + colWidths[endColIdx];
        double cellWidth = cellEndX - cellStartX;

        if (cellWidth > 0.0) {
            double actualHeight = processRtfCell(cell, cellStartX, cellStartTopY, cellWidth, 0.0);
            maxRowHeight = std::max<double>(maxRowHeight, actualHeight);
        }
    }

    if (maxRowHeight <= 0.0) maxRowHeight = estimatedRowHeight;

    // 3. Desenare borduri
    double yBottomPdf = m_pageHeight - cellStartTopY - maxRowHeight;
    double yTopPdf = m_pageHeight - cellStartTopY;

    for (size_t j = 0; j < row.cells.size(); ++j) {
        const RtfCell& cell = row.cells[j];

        if (cell.isMergeNext) continue;

        double cellStartX = (j == 0) ? tableMarginLeftPt : (tableMarginLeftPt + colWidths[j - 1]);
        size_t endColIdx = std::min<size_t>(j + cell.colspan - 1, colWidths.size() - 1);
        double cellEndX = tableMarginLeftPt + colWidths[endColIdx];

        renderCellBorder(cell.borders.left, cellStartX, yTopPdf, cellStartX, yBottomPdf);
        renderCellBorder(cell.borders.right, cellEndX, yTopPdf, cellEndX, yBottomPdf);
        renderCellBorder(cell.borders.top, cellStartX, yTopPdf, cellEndX, yTopPdf);
        renderCellBorder(cell.borders.bottom, cellStartX, yBottomPdf, cellEndX, yBottomPdf);
    }

    // 4. Avansare cursor Y
    m_currentY = cellStartTopY + maxRowHeight;
}
*/

void RtfToPdfConverter::processRtfRow(const RtfRow& row, const std::vector<double>& colWidths) {
    if (row.cells.empty() || colWidths.empty()) return;

    double tableMarginLeftPt = m_marginLeft;
    const double fallbackRowHeight = 15.0;

    // 1. Calculăm înălțimea estimată a întregului rând
    double estimatedRowHeight = calculateRowHeight(row, colWidths);
    if (estimatedRowHeight <= 0.0) estimatedRowHeight = fallbackRowHeight;

    // ⭐ 2. VERIFICARE PAGINARE ÎNAINTE DE A ÎNCEPE DESENAREA CELULELOR!
    // Dacă re-randăm antetul (m_isRenderingHeader == true), nu mai verificăm paginarea din nou
    if (!m_isRenderingHeader) {
        newLineAndCheckPageBreak(estimatedRowHeight);
    }

    // Salvador Y-ul de start DUPĂ eventuala schimbare de pagină!
    double cellStartTopY = m_currentY;

    // 3. Randare celule și calcul înălțime reală rând
    double maxRowHeight = 0.0;

    for (size_t j = 0; j < row.cells.size(); ++j) {
        const RtfCell& cell = row.cells[j];

        // Dacă celula este continuarea unei comasări (\clmrg), o sărim
        if (cell.isMergeNext) continue;

        double cellStartX = (j == 0) ? tableMarginLeftPt : (tableMarginLeftPt + colWidths[j - 1]);

        // Calculăm poziția finală luând în calcul colspan-ul
        size_t endColIdx = std::min<size_t>(j + cell.colspan - 1, colWidths.size() - 1);
        double cellEndX = tableMarginLeftPt + colWidths[endColIdx];
        double cellWidth = cellEndX - cellStartX;

        if (cellWidth > 0.0) {
            int pageBeforeCell = m_currentPageNumber;

            double actualHeight = processRtfCell(cell, cellStartX, cellStartTopY, cellWidth, 0.0);

            // ⭐ Dacă celula a schimbat totuși pagina în interiorul ei, 
            // actualizăm cellStartTopY pentru celulele rămase din acest rând
            if (m_currentPageNumber > pageBeforeCell) {
                cellStartTopY = m_currentY;
            }

            maxRowHeight = std::max<double>(maxRowHeight, actualHeight);
        }
    }

    if (maxRowHeight <= 0.0) maxRowHeight = fallbackRowHeight;

    // 4. Desenare borduri
    double yBottomPdf = m_pageHeight - cellStartTopY - maxRowHeight;
    double yTopPdf = m_pageHeight - cellStartTopY;

    for (size_t j = 0; j < row.cells.size(); ++j) {
        const RtfCell& cell = row.cells[j];

        if (cell.isMergeNext) continue;

        double cellStartX = (j == 0) ? tableMarginLeftPt : (tableMarginLeftPt + colWidths[j - 1]);
        size_t endColIdx = std::min<size_t>(j + cell.colspan - 1, colWidths.size() - 1);
        double cellEndX = tableMarginLeftPt + colWidths[endColIdx];

        renderCellBorder(cell.borders.left, cellStartX, yTopPdf, cellStartX, yBottomPdf);
        renderCellBorder(cell.borders.right, cellEndX, yTopPdf, cellEndX, yBottomPdf);
        renderCellBorder(cell.borders.top, cellStartX, yTopPdf, cellEndX, yTopPdf);
        renderCellBorder(cell.borders.bottom, cellStartX, yBottomPdf, cellEndX, yBottomPdf);
    }

    // 5. Avansare cursor Y pentru rândul următor
    m_currentY = cellStartTopY + maxRowHeight;
}

/*
double RtfToPdfConverter::processRtfCell(const RtfCell& cell, double cellStartX, double cellStartY, double cellWidth, double cellHeight) {
    // Loghează poziția și lățimea REALĂ a celulei.
    //LOG_DEBUG(L"Randare celulă tabel RTF - Incepe la X=" + std::to_wstring(cellStartX) + L", W=" + std::to_wstring(cellWidth));

    // Salvează starea globală a convertorului înainte de a o modifica pentru celulă.
    double original_marginLeft = m_marginLeft;
    double original_contentWidth = m_contentWidth;
    double original_currentY = m_currentY; // Salvează cursorul global

    double totalCellHeightUsed = 0.0;
    // NU salvăm original_currentY, deoarece processRtfParagraph va avansa cursorul Y 
    // în interiorul celulei, iar processRtfRow va decide înălțimea finală a rândului 
    // pe baza maxRowHeight, nu pe baza înălțimii randate a celulei.

    // ⭐ 1. APLICAREA PADDING-ULUI (Conversie din Twips în Puncte PDF)

    // NOTĂ: Dacă folosiți ConvertUtils, presupun că twipsToPoints este disponibil.
    double paddingLeftPt = ConvertUtils::twipsToPoints(cell.padding.leftTwips);
    double paddingRightPt = ConvertUtils::twipsToPoints(cell.padding.rightTwips);
    double paddingTopPt = ConvertUtils::twipsToPoints(cell.padding.topTwips);
    double paddingBottomPt = ConvertUtils::twipsToPoints(cell.padding.bottomTwips);

    // 2. Setează limitele de randare pentru conținutul celulei

    // a) Noul m_marginLeft (Poziția de start a textului)
    // = Granița stânga a celulei + Padding Stânga
    m_marginLeft = cellStartX + paddingLeftPt;

    // b) Noua m_contentWidth (Lățimea reală disponibilă pentru text)
    // = Lățimea totală a celulei - Padding Stânga - Padding Dreapta
    m_contentWidth = cellWidth - paddingLeftPt - paddingRightPt;

    // c) Noul m_currentY (Poziția de start a primei linii de text)
    // = Partea de sus a rândului + Padding Sus
    m_currentY = cellStartY + paddingTopPt;

    // Verificare pentru lățime negativă
    if (m_contentWidth <= 0.0) {
        LOG_ERROR(L"Lățime de conținut negativă/zero din cauza padding-ului excesiv. Sărire celulă.");
        // Restaurare stare
        m_marginLeft = original_marginLeft;
        m_contentWidth = original_contentWidth;
        m_currentY = original_currentY;
        return 0.0;
    }

    // 2. Procesează blocurile de conținut ale celulei
    for (const auto& block : cell.content) {
        if (const RtfParagraph* paragraph = dynamic_cast<const RtfParagraph*>(block.get())) {
            // Randarea paragrafului folosește NOUA m_marginLeft și NOUA m_contentWidth.
            // De asemenea, processRtfParagraph avansează m_currentY.
            double height = processRtfParagraph(*paragraph);
            totalCellHeightUsed += height; // Acumulează înălțimea
        }
        // ... alte tipuri de block (liste etc.)
    }

    // 4. Calculul Înălțimii Totale Folosite (cu Padding)
    // Înălțimea totală de care are nevoie celula (folosită pentru maxRowHeight)
    // = Padding Sus + Înălțimea Randată a Conținutului + Padding Jos
    totalCellHeightUsed += paddingTopPt + paddingBottomPt;

    // 5. Restaurarea stării globale la valorile de dinaintea celulei.
    m_marginLeft = original_marginLeft;
    m_contentWidth = original_contentWidth;
    m_currentY = original_currentY;

    // 6. Returnează înălțimea totală de care a avut nevoie celula (cu padding).
    return totalCellHeightUsed;
}
*/
double RtfToPdfConverter::processRtfCell(const RtfCell& cell, double cellStartX, double cellStartY, double cellWidth, double cellHeight) {
    double original_marginLeft = m_marginLeft;
    double original_contentWidth = m_contentWidth;
    double original_currentY = m_currentY;
    int startPageNumber = m_currentPageNumber; // ⭐ Salvăm numărul paginii de start

    double paddingLeftPt = ConvertUtils::twipsToPoints(cell.padding.leftTwips);
    double paddingRightPt = ConvertUtils::twipsToPoints(cell.padding.rightTwips);
    double paddingTopPt = ConvertUtils::twipsToPoints(cell.padding.topTwips);
    double paddingBottomPt = ConvertUtils::twipsToPoints(cell.padding.bottomTwips);

    m_marginLeft = cellStartX + paddingLeftPt;
    m_contentWidth = cellWidth - paddingLeftPt - paddingRightPt;
    m_currentY = cellStartY + paddingTopPt;

    if (m_contentWidth <= 0.0) {
        m_marginLeft = original_marginLeft;
        m_contentWidth = original_contentWidth;
        m_currentY = original_currentY;
        return 0.0;
    }

    double totalCellHeightUsed = 0.0;
    for (const auto& block : cell.content) {
        if (const RtfParagraph* paragraph = dynamic_cast<const RtfParagraph*>(block.get())) {
            double height = processRtfParagraph(*paragraph);
            totalCellHeightUsed += height;
        }
    }

    totalCellHeightUsed += paddingTopPt + paddingBottomPt;

    // Restaurăm marginile orizontale
    m_marginLeft = original_marginLeft;
    m_contentWidth = original_contentWidth;

    // ⭐ CORECȚIE CRITICĂ: Restaurăm m_currentY DOAR dacă am rămas pe aceeași pagină!
    // Dacă s-a schimbat pagina, păstrăm m_currentY de pe noua pagină.
    if (m_currentPageNumber == startPageNumber) {
        m_currentY = original_currentY;
    }

    return totalCellHeightUsed;
}

void RtfToPdfConverter::processRtfSpan(const RtfSpan& span) {
    // Nu ar trebui să fie apelate direct.
}

void RtfToPdfConverter::applyStyleToWriter(const Style& style) {
    // Nu face nimic în această implementare (stateless styling).
}


void RtfToPdfConverter::renderCellBorder(
    const BorderSpec& spec,
    double x1, double y1, double x2, double y2)
{
    // Verificăm dacă bordura este setată și este vizibilă
    if (spec.isSet() && spec.style != RtfBorderStyle::None) {

        double widthPt = static_cast<double>(spec.widthTwips) / 20.0;

        // Dacă grosimea nu este definită explicit, aplicăm o valoare implicită vizibilă
        if (widthPt <= 0.0) {
            widthPt = 0.5;
        }

        ColorRgb blackColor = { 0.0, 0.0, 0.0 };

        RenderInstruction instruction;

        // 1. Punctul de start (X1, Y1)
        instruction.x = x1;
        instruction.y = y1;

        // 2. Punctul de final (X2, Y2) transmis prin câmpurile width și height
        instruction.width = x2;
        instruction.height = y2;

        // 3. Grosimea și culoarea liniei stocate în Style
        instruction.style.boxModel.borderLeftWidth = widthPt;
        instruction.style.borderColor = blackColor;

        // 4. Tipul instrucțiunii
        instruction.renderFunction = L"line";

        // Adăugare în coada paginii curente
        m_renderQueue.push_back(instruction);
    }
}


void RtfToPdfConverter::renderFooter() {
    if (m_rtfDocument.getFooterBlocks().empty()) return;

    const double TWIPS_PER_POINT = 20.0;
    double marginBottomPt = m_rtfDocument.getPageInfo().getMarginBottomTwips() / TWIPS_PER_POINT;

    // ⭐ CORECȚIE CRITICĂ: Pozitionăm Y-ul de start al subsolului 
    // mai sus cu înălțimea totală a blocurilor sale.
    double footerHeight = getFooterHeight();
    double currentFooterY = m_pageHeight - marginBottomPt - footerHeight + 5.0;

    double originalY = m_currentY;
    m_currentY = currentFooterY;

    double lineHeight = 8.0 * 1.2;

    for (const auto& block : m_rtfDocument.getFooterBlocks()) {
        double heightConsumed = 0.0;

        if (const RtfParagraph* paragraph = dynamic_cast<const RtfParagraph*>(block.get())) {
            heightConsumed = renderFooterParagraph(*paragraph, lineHeight);
        }
        else if (const RtfTable* table = dynamic_cast<const RtfTable*>(block.get())) {
            heightConsumed = renderFooterTable(*table);
        }

        m_currentY += heightConsumed;
    }

    m_currentY = originalY;
}

// În RtfToPdfConverter.cpp
double RtfToPdfConverter::renderFooterTable(const RtfTable& table) {
    if (table.rows.empty()) return 0.0;

    const RtfRow& row = table.rows[0];
    double currentX = m_marginLeft;
    double yBaseline = m_pageHeight - m_currentY;
    double maxRowHeight = 0.0; // Pentru a ști cât de mult să avansăm Y-ul

    // 1. Randarea celulelor rândului (Rândul este deja în footer)
    for (size_t i = 0; i < row.cells.size(); ++i) {
        const RtfCell& cell = row.cells[i];

        // Calculul lățimii celulei
        double cellWidth = 0.0;
        if (i < table.columnWidthsPt.size()) {
            // Lățimea celulei este diferența dintre marginea dreaptă a celulei N+1 și N
            cellWidth = table.columnWidthsPt[i] - (i > 0 ? table.columnWidthsPt[i - 1] : 0.0);
        }
        else {
            // Aceasta e o eroare de parsare/configurare RTF (ar trebui să existe un cellx final)
            continue;
        }

        double cellStartAbsX = currentX; // Poziția X absolută de unde începe celula

        // 2. Randarea conținutului celulei
        for (const auto& block : cell.content) {
            if (const RtfParagraph* paragraph = dynamic_cast<const RtfParagraph*>(block.get())) {

                // Măsoară textul din paragraf (pe un singur rând)
                double contentWidth = 0.0;
                for (const auto& span : paragraph->spans) {
                    contentWidth += m_pdfWriter.measureTextWidth(span.text, span.style);
                }

                // A. Calculează X de start al textului în interiorul celulei (pentru aliniere: Left, Center, Right)
                double x_start_in_cell = cellStartAbsX + calculateXOffsetForAlignment(
                    paragraph->style.textAlign,
                    cellWidth,
                    contentWidth
                );

                double drawX = x_start_in_cell;

                // B. Desenează span-urile
                for (const auto& span : paragraph->spans) {
                    // 💡 IMPORTANT: Trebuie să substituiți câmpurile RTF (\chpgn, \field) aici
                    std::wstring textToDraw = span.text;

                    // Substituție câmpuri (ex: Pag. N / M)
                    textToDraw = replaceRtfFields(textToDraw, m_currentPageNumber, m_totalPagesCount);

                    RenderInstruction instruction;
                    instruction.x = drawX;
                    instruction.y = yBaseline;
                    instruction.text_content = textToDraw;
                    instruction.style = span.style;
                    instruction.renderFunction = L"text";

                    identifyGlobalVars(instruction.text_content, instruction.globalVars);

                    m_renderQueue.push_back(instruction);

                    //pdfWriter.addTextWithSyle(drawX, yBaseline, textToDraw, span.style);
                    drawX += m_pdfWriter.measureTextWidth(textToDraw, span.style);
                }

                // C. Actualizează înălțimea maximă (pentru rând)
                maxRowHeight = std::max<double>(maxRowHeight, paragraph->style.fontSize * 1.2);
            }
        }

        // 3. Avansăm la poziția X de start a următoarei celule
        currentX = cellStartAbsX + cellWidth;
    }

    // Returnăm înălțimea maximă a rândului
    return maxRowHeight > 0.0 ? maxRowHeight : 12.0;
}

// 💡 Funcția ajutătoare (Placeholder, trebuie implementată de dvs.)
double RtfToPdfConverter::calculateXOffsetForAlignment(const std::wstring& align, double cellWidth, double contentWidth) {
    if (align == L"center") {
        return (cellWidth - contentWidth) / 2.0;
    }
    else if (align == L"right") {
        return cellWidth - contentWidth;
    }
    // Default: left
    return 0.0;
}

// 💡 Funcția ajutătoare (Placeholder, trebuie implementată de dvs.)


std::wstring RtfToPdfConverter::replaceRtfFields(const std::wstring& text, int currentPage, int totalPages) {
    std::wstring result = text;

    // Înlocuim doar \chpgn cu pagina curentă
    size_t pos = 0;
    while ((pos = result.find(L"\\chpgn", pos)) != std::wstring::npos) {
        std::wstring pageStr = std::to_wstring(currentPage);
        result.replace(pos, 6, pageStr);
        pos += pageStr.length();
    }

    // ⭐ Nu mai înlocuim \numpages aici! Îl lăsăm intact pentru post-procesare.
    return result;
}

// RtfToPdfConverter::renderFooterParagraph (Versiune simplificată și corectată)
double RtfToPdfConverter::renderFooterParagraph(const RtfParagraph& paragraph, double lineHeight) {
    double heightConsumed = lineHeight;

    // 1. Măsoară lățimea totală a textului din paragraf (aplicând SUBSTITUȚIA)
    double totalTextWidth = 0.0;
    for (const auto& span : paragraph.spans) {
        std::wstring textToDraw = span.text;
        // Aplică substituția pentru o măsurare precisă
        textToDraw = replaceRtfFields(textToDraw, m_currentPageNumber, m_totalPagesCount);
        totalTextWidth += m_pdfWriter.measureTextWidth(textToDraw, span.style);
    }

    // 2. Calculează X de start (bazat pe totalTextWidth substituit)
    double x_start_for_rendering = calculateLineXStart(
        paragraph.style.textAlign,
        m_contentWidth,
        totalTextWidth,
        m_marginLeft
    );

    double currentX = x_start_for_rendering;
    double yBaseline = m_pageHeight - m_currentY;

    // 3. Iterează și desenează (aplicând din nou SUBSTITUȚIA)
    for (const auto& span : paragraph.spans) {
        const Style& spanStyle = span.style;

        // ⭐ Pasul cheie: Substituție pentru randare
        
        
        std::wstring textToDrawFinal = replaceRtfFields(
            span.text,
            m_currentPageNumber,
            m_totalPagesCount
        );
        
        //std::wstring textToDrawFinal = span.text;
        if (textToDrawFinal == L"\\t") {

            // 1. Identifică următoarea oprire de tabulator (din paragraph.style)
            // Logica complexă presupune:
            //    a) Găsirea celei mai apropiate opriri (în puncte) după currentX.
            //    b) Aplicarea tipului de aliniere (stânga, centru, dreapta)
            /*
            double nextTabStopX = findNextTabStopPosition(
                paragraph.style.tabStops, // Opririle definite în RTF, convertite în PT
                currentX
            );

            // 2. Setează noua poziție X. 
            // Dacă nu găsiți o oprire de tabulator, folosiți un avans standard (ex: 36pt).
            if (nextTabStopX > currentX) {
                currentX = nextTabStopX;
            }
            else {
                // Fără oprire explicită, folosește un salt implicit (ex: 0.5 inchi = 36pt)
                currentX += 36.0;
            }
            */
            currentX += TAB_WIDTH;
            // Săriți peste randarea caracterului \t
            continue;
        }

        // Randarea textului span-ului
        
        RenderInstruction instruction;
        instruction.x = currentX;
        instruction.y = yBaseline;
        instruction.text_content = textToDrawFinal;
        instruction.style = spanStyle;
        instruction.renderFunction = L"text";

        identifyGlobalVars(instruction.text_content, instruction.globalVars);

        m_renderQueue.push_back(instruction);

        //pdfWriter.addTextWithSyle(currentX, yBaseline, textToDrawFinal, spanStyle);

        // Avansăm cursorul X folosind lățimea textului SUBSTITUIT!
        double spanWidth = m_pdfWriter.measureTextWidth(textToDrawFinal, spanStyle);
        currentX += spanWidth;
    }

    return heightConsumed;
}



bool RtfToPdfConverter::finalizeAndPaint() {
    int current_render_page = 0;

    for (const auto& instruction : m_renderQueue) {
        if (instruction.renderFunction == L"startPage") {
            current_render_page++;
            m_pdfWriter.startPage(instruction.width, instruction.height);
        }
        else if (instruction.renderFunction == L"endPage") {
            m_pdfWriter.endPage();
        }
        else if (instruction.renderFunction == L"text") {
            std::wstring final_text_to_draw = instruction.text_content;

            // 1. Înlocuire token RTF special pentru numărul paginii (\chpgn)
            size_t chpgnPos = final_text_to_draw.find(L"\\chpgn");
            if (chpgnPos != std::wstring::npos) {
                final_text_to_draw.replace(chpgnPos, 7, std::to_wstring(current_render_page));
            }

            // 2. Substituția variabilelor globale (ex: $operator, page, total, etc.)
            for (const auto& varName : instruction.globalVars) {
                auto it = m_globalVarResolvers.find(varName);
                if (it != m_globalVarResolvers.end()) {
                    std::wstring resolvedValue = it->second(current_render_page);
                    size_t pos = 0;
                    while ((pos = final_text_to_draw.find(varName, pos)) != std::wstring::npos) {
                        final_text_to_draw.replace(pos, varName.length(), resolvedValue);
                        pos += resolvedValue.length();
                    }
                }
            }

            LOG_DEBUG(L"[RTF2PDF] Desenez textul: \"" + final_text_to_draw +
                L"\" la coordonatele: X=" + std::to_wstring(instruction.x) +
                L", Y=" + std::to_wstring(instruction.y));

            m_pdfWriter.addTextWithSyle(
                instruction.x,
                instruction.y,
                final_text_to_draw,
                instruction.style
            );
        }
        else if (instruction.renderFunction == L"line") {
            double x2 = instruction.width;
            double y2 = instruction.height;
            double thickness = instruction.style.boxModel.borderLeftWidth;
            ColorRgb color = instruction.style.borderColor;

            m_pdfWriter.addLine(
                instruction.x,
                instruction.y,
                x2,
                y2,
                thickness,
                color
            );
        }
    }

    return true;
}

void RtfToPdfConverter::initializeGlobalVarResolvers() {
    // Folosim o funcție lambda care captează m_totalPagesCount final

    // 1. NUMPAGES: Variabilă globală, valoare cunoscută la final.
    m_globalVarResolvers[L"NUMPAGES"] = [this](int page_index) -> std::wstring {
        // m_totalPagesCount este setat la finalul Pass-ului 1.
        return std::to_wstring(this->m_totalPagesCount);
    };

    // 2. \chpgn (Page Number): Variabilă care depinde de context (pagina curentă).
    m_globalVarResolvers[L"\\chpgn"] = [](int page_index) -> std::wstring {
        // Folosim indexul paginii primit ca parametru (din RenderInstruction::page_number)
        return std::to_wstring(page_index);
    };

    // (Aici ați adăuga și alte câmpuri: L"DATE", L"TIME", etc.)
}


void RtfToPdfConverter::identifyGlobalVars(const std::wstring& text, std::vector<std::wstring>& vars) {
    // Note: Folosim find() pentru a verifica prezența câmpului RTF

    // 1. NUMPAGES
    if (text.find(L"NUMPAGES") != std::wstring::npos) {
        vars.push_back(L"NUMPAGES");
    }

    // 2. \chpgn (Page Number)
 //   if (text.find(L"\\chpgn") != std::wstring::npos) {
//        vars.push_back(L"\\chpgn");
//    }

    // 3. (Adăugați și alte câmpuri globale dacă e necesar: DATE, TIME, etc.)
}




void RtfToPdfConverter::renderHeader() {
    if (m_rtfDocument.getHeaderBlocks().empty()) return;

    double currentHeaderY = 15.0; // Marginea de sus a paginii
    m_currentY = currentHeaderY;

    for (const auto& block : m_rtfDocument.getHeaderBlocks()) {
        if (!block) continue;

        if (const RtfParagraph* paragraph = dynamic_cast<const RtfParagraph*>(block.get())) {
            // Adăugăm un mic spațiu (4pt) înainte de paragraf pentru a nu fi lipit de tabelul de sus
            if (m_currentY > currentHeaderY) {
                m_currentY += 4.0;
            }

            double h = renderHeaderParagraph(*paragraph, 0.0);

            // Avansăm cursorul Y cu înălțimea paragrafului + spațiu mic (4pt) până la tabelul următor
            m_currentY += h + 4.0;
        }
        else if (const RtfTable* table = dynamic_cast<const RtfTable*>(block.get())) {
            renderHeaderTable(*table);
        }
    }
}
 
double RtfToPdfConverter::renderHeaderParagraph(const RtfParagraph& paragraph, double lineHeight) {
    if (paragraph.spans.empty()) return 0.0;

    auto replaceFields = [this](const std::wstring& str) {
        return this->replaceRtfFields(str, m_currentPageNumber, m_totalPagesCount);
    };

    double fontSize = paragraph.style.fontSize > 0 ? paragraph.style.fontSize : 10.0;
    double actualLineHeight = fontSize * 1.2;

    // Descompunem paragraful în linii logice (pentru a gestiona stilurile mixte \b / \b0 pe aceeași linie)
    std::vector<ParagraphLine> lines = buildParagraphLines(paragraph, replaceFields);
    if (lines.empty()) return 0.0;

    double paragraphY = m_currentY;

    for (const auto& line : lines) {
        // 1. Măsurăm lățimea totală a liniei
        double totalLineWidth = 0.0;
        for (const auto& chunk : line.chunks) {
            if (chunk.text == L"\\t") {
                totalLineWidth += TAB_WIDTH;
            }
            else {
                totalLineWidth += m_pdfWriter.measureTextWidth(chunk.text, chunk.style);
            }
        }

        // 2. Calculăm punctul X de start în funcție de alinierea paragrafului
        double drawX = calculateLineXStart(
            paragraph.style.textAlign,
            m_contentWidth,
            totalLineWidth,
            m_marginLeft
        );

        // ⭐ CORECȚIA CRITICĂ Y-BASELINE:
        // Coborâm linia de bază cu dimensiunea fontului (fontSize) sub paragraphY
        double yBaseline = m_pageHeight - paragraphY - fontSize;

        // 3. Deseneăm bucățile de text din linie secvențial
        for (const auto& chunk : line.chunks) {
            if (chunk.text == L"\\t") {
                drawX += TAB_WIDTH;
                continue;
            }

            RenderInstruction instruction;
            instruction.x = drawX;
            instruction.y = yBaseline;
            instruction.text_content = chunk.text;
            instruction.style = chunk.style;
            instruction.renderFunction = L"text";

            identifyGlobalVars(instruction.text_content, instruction.globalVars);
            m_renderQueue.push_back(instruction);

            drawX += m_pdfWriter.measureTextWidth(chunk.text, chunk.style);
        }

        paragraphY += actualLineHeight;
    }

    return lines.size() * actualLineHeight;
}


static std::vector<std::wstring> splitByLineBreak(const std::wstring& str) {
    std::vector<std::wstring> lines;
    std::wstring token = L"$line$";
    size_t start = 0;
    size_t end = str.find(token);
    while (end != std::wstring::npos) {
        lines.push_back(str.substr(start, end - start));
        start = end + token.length();
        end = str.find(token, start);
    }
    lines.push_back(str.substr(start));
    return lines;
}


static std::vector<ParagraphLine> buildParagraphLines(
    const RtfParagraph& paragraph,
    const std::function<std::wstring(const std::wstring&)>& textReplacer)
{
    std::vector<ParagraphLine> lines;
    lines.emplace_back(); // Linia inițială

    for (const auto& span : paragraph.spans) {
        std::wstring text = textReplacer(span.text);
        std::vector<std::wstring> subStrings = splitByLineBreak(text);

        for (size_t i = 0; i < subStrings.size(); ++i) {
            if (i > 0) {
                lines.emplace_back(); // Trecere la o linie nouă cauzată de $line$
            }
            if (!subStrings[i].empty()) {
                lines.back().chunks.push_back({ subStrings[i], span.style });
            }
        }
    }
    return lines;
}

double RtfToPdfConverter::renderHeaderTable(const RtfTable& table) {
    if (table.rows.empty()) return 0.0;

    double totalTableHeight = 0.0;

    // Lambda utilitar pentru substituția câmpurilor RTF
    auto replaceFields = [this](const std::wstring& str) {
        return this->replaceRtfFields(str, m_currentPageNumber, m_totalPagesCount);
    };

    for (size_t r = 0; r < table.rows.size(); ++r) {
        const RtfRow& row = table.rows[r];
        double rowStartY = m_currentY;

        // -------------------------------------------------------------
        // PASUL 1: Calculăm înălțimea maximă a rândului curent (maxRowHeight)
        // -------------------------------------------------------------
        double maxRowHeight = 0.0;
        for (size_t i = 0; i < row.cells.size(); ++i) {
            const RtfCell& cell = row.cells[i];
            double cellWidth = (i < table.columnWidthsPt.size())
                ? (table.columnWidthsPt[i] - (i > 0 ? table.columnWidthsPt[i - 1] : 0.0))
                : 0.0;

            if (cellWidth <= 0.0) continue;

            double cellContentHeight = 0.0;
            for (const auto& block : cell.content) {
                if (const RtfParagraph* paragraph = dynamic_cast<const RtfParagraph*>(block.get())) {
                    double fontSize = paragraph->style.fontSize > 0 ? paragraph->style.fontSize : 9.0;
                    double lineHeight = fontSize * 1.2;

                    std::vector<ParagraphLine> lines = buildParagraphLines(*paragraph, replaceFields);
                    cellContentHeight += lines.size() * lineHeight;
                }
            }
            maxRowHeight = std::max<double>(maxRowHeight, cellContentHeight);
        }

        double rowH = maxRowHeight > 0.0 ? maxRowHeight : 15.0;
        double rowEndY = rowStartY + rowH;

        // Coordonatele Y în sistemul PDF (Y=0 jos)
        double yTop = m_pageHeight - rowStartY;
        double yBottom = m_pageHeight - rowEndY;

        // -------------------------------------------------------------
        // PASUL 2: Randăm conținutul și BORDURILE pentru fiecare celulă
        // -------------------------------------------------------------
        double currentX = m_marginLeft;

        for (size_t i = 0; i < row.cells.size(); ++i) {
            const RtfCell& cell = row.cells[i];

            double cellWidth = (i < table.columnWidthsPt.size())
                ? (table.columnWidthsPt[i] - (i > 0 ? table.columnWidthsPt[i - 1] : 0.0))
                : 0.0;

            if (cellWidth <= 0.0) continue;

            double xLeft = currentX;
            double xRight = currentX + cellWidth;
            double cellY = rowStartY;

            // --- A. Randare Text ---
            for (const auto& block : cell.content) {
                if (const RtfParagraph* paragraph = dynamic_cast<const RtfParagraph*>(block.get())) {
                    double fontSize = paragraph->style.fontSize > 0 ? paragraph->style.fontSize : 9.0;
                    double lineHeight = fontSize * 1.2;

                    std::vector<ParagraphLine> lines = buildParagraphLines(*paragraph, replaceFields);

                    for (const auto& line : lines) {
                        // 1. Măsurăm lățimea totală a liniei curente (însumând toate bucățile/span-urile ei)
                        double totalLineWidth = 0.0;
                        for (const auto& chunk : line.chunks) {
                            if (chunk.text == L"\\t") {
                                totalLineWidth += TAB_WIDTH;
                            }
                            else {
                                totalLineWidth += m_pdfWriter.measureTextWidth(chunk.text, chunk.style);
                            }
                        }

                        // 2. Calculeăm X de start pentru întreaga linie în funcție de aliniere (stânga, centru, dreapta)
                        double drawX = xLeft + calculateXOffsetForAlignment(
                            paragraph->style.textAlign,
                            cellWidth,
                            totalLineWidth
                        );

                        double yBaseline = m_pageHeight - cellY - fontSize;

                        // 3. Desenăm secvențial bucățile de text din linie
                        for (const auto& chunk : line.chunks) {
                            if (chunk.text == L"\\t") {
                                drawX += TAB_WIDTH;
                                continue;
                            }

                            RenderInstruction instruction;
                            instruction.x = drawX;
                            instruction.y = yBaseline;
                            instruction.text_content = chunk.text;
                            instruction.style = chunk.style;
                            instruction.renderFunction = L"text";

                            identifyGlobalVars(instruction.text_content, instruction.globalVars);
                            m_renderQueue.push_back(instruction);

                            // Avansăm X-ul orizontal cu lățimea bucății tocmai desenate
                            drawX += m_pdfWriter.measureTextWidth(chunk.text, chunk.style);
                        }

                        // Trecem la linia următoare doar după finalizarea tuturor bucăților din linia curentă
                        cellY += lineHeight;
                    }
                }
            }

            // --- B. Randare Borduri Celulă ---
            if (cell.borders.top.isSet()) {
                renderCellBorder(cell.borders.top, xLeft, yTop, xRight, yTop);
            }
            if (cell.borders.bottom.isSet()) {
                renderCellBorder(cell.borders.bottom, xLeft, yBottom, xRight, yBottom);
            }
            if (cell.borders.left.isSet()) {
                renderCellBorder(cell.borders.left, xLeft, yTop, xLeft, yBottom);
            }
            if (cell.borders.right.isSet()) {
                renderCellBorder(cell.borders.right, xRight, yTop, xRight, yBottom);
            }

            currentX = xRight;
        }

        totalTableHeight += rowH;
        m_currentY += rowH; // Avansăm cursorul Y pentru următorul rând din tabel
    }

    return totalTableHeight;
}


double RtfToPdfConverter::getFooterHeight() const {
    const auto& footerBlocks = m_rtfDocument.getFooterBlocks();
    if (footerBlocks.empty()) return 0.0;

    double totalHeight = 0.0;

    for (const auto& block : footerBlocks) {
        if (!block) continue;

        if (const RtfParagraph* paragraph = dynamic_cast<const RtfParagraph*>(block.get())) {
            double fontSize = paragraph->style.fontSize > 0 ? paragraph->style.fontSize : 8.0;
            totalHeight += fontSize * 1.2;
        }
        else if (const RtfTable* table = dynamic_cast<const RtfTable*>(block.get())) {
            // Un rând de tabel din footer ocupă în general între 12pt și 15pt
            totalHeight += table->rows.size() * 14.0;
        }
    }

    // Adăugăm un buffer de siguranță de 10pt pentru distanțare de corpul paginii
    return totalHeight > 0.0 ? (totalHeight + 10.0) : 0.0;
}

void RtfToPdfConverter::finalizePageNumbers() {
    for (auto& instruction : m_renderQueue) {
        if (instruction.renderFunction == L"text") {
            size_t pos = 0;
            while ((pos = instruction.text_content.find(L"\\numpages", pos)) != std::wstring::npos) {
                std::wstring totalStr = std::to_wstring(m_totalPagesCount);
                instruction.text_content.replace(pos, 9, totalStr);
                pos += totalStr.length();
            }
        }
    }
}