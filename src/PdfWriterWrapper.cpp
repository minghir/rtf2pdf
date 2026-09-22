
#include "ConsoleManager.hpp" // Pentru logare (opțional, dar util)
#include "PdfWriterWrapper.hpp"

#include <InputStringStream.h> 

#include "PDFImageXObject.h"
#include "PDFFormXObject.h"
#include "JPEGImageHandler.h"
#include "PNGImageHandler.h"
#include "PrimitiveObjectsWriter.h"
#include "PDFObject.h"
#include "InputStringBufferStream.h"
#include "ResourcesDictionary.h"
#include "PDFWriter.h" // Cel mai probabil deja inclus
#include "AbstractContentContext.h"


#include <iomanip>
#include <sstream>
#include <map>
#include <vector>
#include <filesystem>




// Presupunem că ai definit wstring_to_utf8 undeva.
// Dacă nu, trebuie să o definim:
// std::string wstring_to_utf8(const std::wstring& wstr) {
//     // Implementare conversie wstring -> utf8
//     return std::string(wstr.begin(), wstr.end());
// }


PdfWriterWrapper::PdfWriterWrapper() : m_currentPage(nullptr),
m_currentPageContext(nullptr),
m_currentFont(nullptr),
m_defaultFont(nullptr),
m_isBoldDesired(false),
imageCounter(0),
m_isFinalized(false) {
        // ... alte inițializări ...
        //PDFUsedFont* m_defaultFont = nullptr;
        // Inițializarea m_fontFamilies, care trebuie să fie goală la acest punct
        m_fontFamilies["Arial"] = {
            "Fonts\\arial.ttf",
            "Fonts\\arialbd.ttf",
            "Fonts\\ariali.ttf",
            "Fonts\\arialbi.ttf"
        };


        m_fontFamilies["Times New Roman"] = {
            "Fonts\\times.ttf",
            "Fonts\\timesbd.ttf",
            "Fonts\\timesi.ttf",
            "Fonts\\timesbi.ttf"
        };

        m_fontFamilies["Arial Narrow"] = {
           "Fonts\\ARIALN.ttf",
           "Fonts\\ARIALNB.ttf",
           "Fonts\\ARIALNI.ttf",
           "Fonts\\ARIALNBI.ttf"
        };


        

       
}

/*
PdfWriterWrapper::~PdfWriterWrapper() {
    // 1. Încercarea de finalizare (pentru a închide PDFWriter)
    if (!m_isFinalized) {
        // ... (Logica de apelare a finalize() sau EndPDF())
        // Sau, pur și simplu, lăsați m_writer.reset() să se ocupe de asta.
    }

    m_writer.reset();

    // 2. 🚨 CORECȚIE CRITICĂ: Eliberarea paginii curente neterminate
    if (m_currentPage) {
        LOG_WARNING(L"Ștergerea forțată a paginii PDF neterminate.");
        delete m_currentPage; // ⚠️ AICI SE ELIBEREAZĂ MEMORIA
        m_currentPage = nullptr;
    }

    if (m_currentPageContext) {
        delete m_currentPageContext;
        m_currentPageContext = nullptr;
    }
    delete m_currentFont;
    m_currentFont = nullptr;
    delete m_defaultFont;
    m_defaultFont = nullptr;
    // 3. Curățarea resurselor rămase (Font Cache)
    // Deoarece m_writer deține fonturile, nu trebuie să iterați peste cache-ul dumneavoastră.
    // Dar ar trebui să resetați pointerul inteligent al scriitorului
    
    // ... Alte curățări ...
}
*/
PdfWriterWrapper::~PdfWriterWrapper() {
    if (m_currentPageContext && m_writer) {
        m_writer->EndPageContentContext(m_currentPageContext);
        m_currentPageContext = nullptr;
    }
    if (m_currentPage) {
        delete m_currentPage;
        m_currentPage = nullptr;
    }

    m_fontCache.clear();
    m_currentFont = nullptr;
    m_defaultFont = nullptr;

    // Aici este locul sigur unde m_writer și m_memoryStream sunt eliberate
    m_writer.reset();
    m_memoryStream.reset();
}

// --- Controlul Documentului ---

bool PdfWriterWrapper::initialize(const std::wstring& filePath, double width, double height) {
    if (m_writer) {
        // Dacă m_writer există, și nu este finalizat, ar trebui să fi fost finalizat de finalize().
        // Dar pentru siguranță, distrugem vechiul obiect Hummus.
        m_writer.reset();
    }
    m_writer = std::make_unique<PDFWriter>();

    if (!m_isFinalized) {
        // Dacă nu s-a finalizat documentul anterior, finalizați-l pentru a curăța memoria.
        // Totuși, cel mai bine este ca PdfConverter să gestioneze asta.

        // Dacă initialize este apelat pe un obiect *nou* sau *finalizat*:

        // Asigurați-vă că fontCache este absolut gol înainte de a adăuga fonturi noi
        if (!m_fontCache.empty()) {
            // Logați sau aruncați o eroare. Această stare e greșită la inițializare.
            // Dacă s-a ajuns aici, înseamnă că initialize() a fost apelat pe un obiect 
            // care a rulat un finalize() incomplet sau nu a rulat deloc.
            // Dacă nu ați implementat auto-resetul, apelăm clean:
            // cleanPdfWriterWrapperInternalState(); // O funcție care curăță tot
        }
    }
    // Resetarea stării pentru a începe scrierea:
    m_isFinalized = false;


    // 1. Convertirea căii
    std::string path = wstring_to_utf8(filePath);

    // 2. Inițializarea și start-ul PDF-ului
    //EStatusCode status = m_writer.StartPDF(path.c_str(), ePDFVersion13);

   // if (status != eSuccess) {
   //     LOG_ERROR(L"[PDF Wrapper] Eroare la StartPDF pentru fișier: " + filePath);
   //     return false;
   // }

    PDFCreationSettings creationSettings(
        false, // CompressStreams = false (Dezactivare compresie ZLIB)
        true   // EmbedFonts = true (Presupunem că doriți fonturi încorporate)
        // Lăsăm celelalte opțiuni la valorile implicite (DefaultEncryptionOptions, WriteXrefAsXrefStream=false)
    );

    // 2. Inițializarea și start-ul PDF-ului, folosind noile setări
    // Acesta folosește StartPDF(path, version, LogConfig, PDFCreationSettings)
    EStatusCode status = m_writer->StartPDF(path.c_str(), ePDFVersion13,
        LogConfiguration::DefaultLogConfiguration(),
        creationSettings);

    if (status != eSuccess) {
        LOG_ERROR(L"[PDF Wrapper] Eroare la StartPDF pentru fișier: " + filePath);
        return false;
    }

    // Log de avertizare util
    LOG_WARNING(L"[PDF Wrapper] Compresia stream-urilor a fost dezactivată (CompressStreams=false) din cauza unui conflict ZLIB/JPEG.");

    // 3. Log detaliat cu dimensiuni
    std::wstringstream ss;
    ss << L"[PDF Wrapper] Document PDF inițializat cu succes.\n"
        << L"  - Fișier: " << filePath << L"\n"
        << L"  - Dimensiuni pagină: " << width << L" x " << height;
    LOG_SUCCESS(ss.str());

    // 4. Inițializare pagină (dacă e cazul)
    // startPage(width, height);
    loadAllFonts();
    return true;
}

bool PdfWriterWrapper::initializeToMemory(double width, double height) {
    m_isFinalized = false;
    m_isInMemoryMode = true;

    // 1. Instanțiem writer-ul custom din memorie
    m_memoryStream = std::make_unique<StringWriter>();

    // 2. Instanțiem PDFWriter dacă nu există
    if (!m_writer) {
        m_writer = std::make_unique<PDFWriter>();
    }

    // 3. Pornim generarea PDF în stream
    EStatusCode status = m_writer->StartPDFForStream(
        m_memoryStream.get(),
        ePDFVersion13
    );

    if (status != eSuccess) {
        LOG_FATAL(L"[PDF Wrapper] Eroare fatală la pornirea PDFWriter în memorie.");
        return false;
    }

    // 4. Încărcăm fonturile PENTRU NOUA INSTANȚĂ m_writer
    loadAllFonts();

    if (m_fontCache.empty() && !m_defaultFont) {
        LOG_ERROR(L"[PDF Wrapper] CRITIC: Niciun font nu a putut fi încărcat la initializeToMemory!");
        return false;
    }

    return true;
}

/*
bool PdfWriterWrapper::finalize() {

    if (m_isFinalized) {
        LOG_WARNING(L"[PDF Wrapper] finalize() apelat de doua ori.");
        return true; // sau false, depinde de politica
    }

    if (m_currentPageContext) {
        m_writer->EndPageContentContext(m_currentPageContext);
        m_currentPageContext = nullptr;
    }
    if (m_currentPage) {
        m_writer->WritePageAndRelease(m_currentPage);
        m_currentPage = nullptr;
    }

    EStatusCode status = m_writer->EndPDF();

    // 🌟 CORECȚIE: Fonturile cache-uite TREBUIE eliberate manual.
    if (!m_fontCache.empty()) {

        // 🌟 Schimbare 2: Iterăm și eliberăm. Aici are loc crash-ul.
        // Încercați o iterație C++98/C++03 mai veche, care este uneori mai stabilă 
        // pe platforme cu biblioteci mai vechi sau compilatoare specifice.

        // Versiunea mai sigură, care nu folosește auto const& [key, value]
        for (std::map<pdfFontKey, PDFUsedFont*>::iterator it = m_fontCache.begin();
            it != m_fontCache.end(); ++it)
        {
            PDFUsedFont* fontPtr = it->second;
            if (fontPtr) { // Verificare suplimentară împotriva pointerilor nuli
               //delete fontPtr;
                // După delete, pointerul ar trebui să fie nul, deși nu îl putem seta aici.
            }
        }
        m_fontCache.clear();
    }

    // Resetarea pointerilor interni de urmărire a fontului
    m_currentFont = nullptr;
    m_defaultFont = nullptr;

    m_isFinalized = (status == eSuccess);
  
    return m_isFinalized;
}
*/
bool PdfWriterWrapper::finalize() {
    if (m_isFinalized) {
        LOG_WARNING(L"[PDF Wrapper] finalize() apelat de două ori.");
        return true;
    }

    if (!m_writer) {
        LOG_ERROR(L"[PDF Wrapper] m_writer este nullptr în finalize().");
        return false;
    }

    // 1. Închide contextul paginii curente dacă a rămas deschis
    if (m_currentPageContext) {
        m_writer->EndPageContentContext(m_currentPageContext);
        m_currentPageContext = nullptr;
    }

    // 2. Încheie și eliberează pagina curentă dacă nu a fost procesată
    if (m_currentPage) {
        m_writer->WritePageAndRelease(m_currentPage);
        m_currentPage = nullptr;
    }

    // 3. Finalizează documentul (PDFWriter scrie trailer-ul și xref-ul în m_memoryStream)
    EStatusCode status = m_writer->EndPDF();

    // 🚨 NOTĂ CRITICĂ: Păstrăm m_memoryStream activ! 
    // NU facem m_memoryStream.reset() aici, altfel se distruge buffer-ul de octeți 
    // înainte de a fi extras de convertToMemory(). 
    // Curățarea stream-ului se va face automat în destructorul ~PdfWriterWrapper() 
    // sau la următorul initializeToMemory().

    // 4. Resetăm doar referințele locale de fonturi (fără delete pe pointeri, 
    // deoarece m_writer deține memoria fonturilor)
    m_fontCache.clear();
    m_currentFont = nullptr;
    m_defaultFont = nullptr;

    m_isFinalized = (status == eSuccess);

    if (m_isFinalized) {
        LOG_SUCCESS(L"[PDF Wrapper] Documentul PDF a fost finalizat cu succes.");
    }
    else {
        LOG_ERROR(L"[PDF Wrapper] EndPDF() a eșuat!");
    }

    return m_isFinalized;
}

// --- Controlul Paginilor ---
/*
void PdfWriterWrapper::startPage(double width, double height) {
    // Finalizează pagina anterioară, dacă există
    if (m_currentPageContext) {
        m_writer->EndPageContentContext(m_currentPageContext);
        m_currentPageContext = nullptr;
    }
    if (m_currentPage) {
        m_writer->WritePageAndRelease(m_currentPage);
        m_currentPage = nullptr;
        
    }

    // Crearea paginii noi
    m_currentPage = new PDFPage();
    // Atenție: PDFWriter folosește puncte (72dpi), nu mm, implicit. 
    // Presupunând că 595x842 sunt dimensiunile A4 în puncte.
    m_currentPage->SetMediaBox(PDFRectangle(0, 0, width, height));

    // Inițiază contextul de conținut
    m_currentPageContext = m_writer->StartPageContentContext(m_currentPage);

    LOG_DEBUG(L"[PDF Wrapper] Pagina nouă inițializată.");



}
*/

void PdfWriterWrapper::startPage(double width, double height) {
    // 1. Verificare critică împotriva nullptr
    if (!m_writer) {
        LOG_FATAL(L"[PDF Wrapper] CRASH PREVENTED: m_writer este nullptr în startPage()! Apelul initialize() sau initializeToMemory() a fost omis.");
        return;
    }

    // 2. Finalizează pagina anterioară, dacă există
    if (m_currentPageContext) {
        m_writer->EndPageContentContext(m_currentPageContext);
        m_currentPageContext = nullptr;
    }

    if (m_currentPage) {
        m_writer->WritePageAndRelease(m_currentPage);
        m_currentPage = nullptr;
    }

    // 3. Crearea paginii noi
    m_currentPage = new PDFPage();
    if (!m_currentPage) {
        LOG_FATAL(L"[PDF Wrapper] Nu s-a putut aloca memorie pentru PDFPage.");
        return;
    }

    m_currentPage->SetMediaBox(PDFRectangle(0, 0, width, height));

    // 4. Creează contextul de conținut
    m_currentPageContext = m_writer->StartPageContentContext(m_currentPage);

    if (!m_currentPageContext) {
        LOG_ERROR(L"[PDF Wrapper] StartPageContentContext a returnat nullptr! PDFWriter nu este într-o stare validă de scriere (StartPDF nu a fost apelat).");
        return;
    }

    LOG_DEBUG(L"[PDF Wrapper] Pagina nouă inițializată cu succes.");
}

void PdfWriterWrapper::endPage() {
    // Logica de finalizare a paginii este inclusă în startPage și finalize
    // Dacă vrei să forțezi o pagină nouă explicit:
    // startPage(defaultWidth, defaultHeight); 
}




void PdfWriterWrapper::addText(double x, double y, const std::wstring& text, double fontSize, const ColorRgb& textColor, std::wstring fontFamily) {
    
   // if (!m_currentPageContext || !m_currentFont) return;
    if (!m_currentPageContext) {
        LOG_ERROR(L"PdfWriterWrapper::addText - PDF Context is null");
        return;
    }

    if(fontFamily == L"default"){
        m_currentFont = m_defaultFont;
    }
    else {
        m_currentFont = getCachedFont(fontFamily);
    }
    
    if (!m_currentFont) {
        LOG_ERROR(L"[PDF Wrapper] Imposibil de randat textul - m_currentFont este NULL!");
        return;
    }


    // Asigură-te că ai funcția de conversie (sau implementeaz-o)
    std::string utf8_text = wstring_to_utf8(text);

    long r = static_cast<long>(textColor.r * 255);
    long g = static_cast<long>(textColor.g * 255);
    long b = static_cast<long>(textColor.b * 255);

    long colorValue = (r << 16) | (g << 8) | b;

    // Crearea opțiunilor de text (aici se setează culoarea)
    AbstractContentContext::TextOptions textOptions(
        m_currentFont,
        fontSize,
        AbstractContentContext::eRGB,
        colorValue // Culoarea este aplicată aici
    );

    m_currentPageContext->WriteText(x, y, utf8_text, textOptions);
}


void PdfWriterWrapper::addTextWithSyle(double x, double y, const std::wstring& text, const Style& style) {
    if (!m_currentPageContext) {
        LOG_ERROR(L"PdfWriterWrapper::addTextWithSyle - PDF Context is null");
        return;
    }

    // Convertim fontFamily la UTF-8
    std::wstring fontFamilyW = style.fontFamily.empty() ? L"default" : style.fontFamily;
    std::string fontFamilyUtf8 = wstring_to_utf8(fontFamilyW);

    // Determinăm stilul fontului
    bool isBold = (style.fontWeight == L"bold" || style.fontWeight == L"700");
    bool isItalic = (style.fontStyle == L"italic" || style.fontStyle == L"oblique");

    // Alegem calea către fișierul de font potrivit
    std::string fontPath;
    auto it = m_fontFamilies.find(fontFamilyUtf8);
    if (it != m_fontFamilies.end()) {
        const FontStylePaths& paths = it->second;
        if (isBold && isItalic)
            fontPath = paths.BoldItalic;
        else if (isBold)
            fontPath = paths.Bold;
        else if (isItalic)
            fontPath = paths.Italic;
        else
            fontPath = paths.Regular;
    }

    // Încărcăm fontul dacă nu e deja în cache
    pdfFontKey key{ fontFamilyW, style.fontWeight, style.fontStyle };
    auto cached = m_fontCache.find(key);
    if (cached != m_fontCache.end()) {
        m_currentFont = cached->second;
    }
    else {
        if (!fontPath.empty()) {
            PDFUsedFont* loadedFont = m_writer->GetFontForFile(fontPath);
            if (loadedFont) {
                m_fontCache[key] = loadedFont;
                m_currentFont = loadedFont;
            }
            else {
                LOG_WARNING(L"Fontul nu a putut fi încărcat: " + fontFamilyW);
                m_currentFont = m_defaultFont;
            }
        }
        else {
            LOG_WARNING(L"Fontul nu a fost găsit în m_fontFamilies: " + fontFamilyW);
            m_currentFont = m_defaultFont;
        }
    }

    // Convertim textul la UTF-8
    std::string utf8_text = wstring_to_utf8(text);

    // Calculăm culoarea
    long r = static_cast<long>(style.textColor.r * 255);
    long g = static_cast<long>(style.textColor.g * 255);
    long b = static_cast<long>(style.textColor.b * 255);
    long colorValue = (r << 16) | (g << 8) | b;

    // Setăm opțiunile de text
    double fontSize = (style.fontSize > 0.0) ? style.fontSize : 12.0;
    AbstractContentContext::TextOptions textOptions(
        m_currentFont,
        fontSize,
        AbstractContentContext::eRGB,
        colorValue
    );

    // Scriem textul
    m_currentPageContext->WriteText(x, y, utf8_text, textOptions);

    // Aplicăm textDecoration dacă e cazul
    if (style.textDecoration == L"underline" || style.textDecoration == L"line-through") {
        double textWidth = m_currentFont->CalculateTextAdvance(utf8_text, fontSize);
        double lineY = y;

        if (style.textDecoration == L"underline") {
            lineY -= fontSize * 0.1;
        }
        else if (style.textDecoration == L"line-through") {
            lineY += fontSize * 0.3;
        }

        // Folosim metoda dedicată pentru a desena linia
        addLine(x, lineY, x + textWidth, lineY, 1.0, style.textColor);
    }
}




void PdfWriterWrapper::addLine(double x1, double y1, double x2, double y2, double thickness, const ColorRgb& color) {
    if (!m_currentPageContext) {
        LOG_ERROR(L"PdfWriterWrapper::DrawLine - PDF Context is null");
        return;
    }

    // Convertim culoarea RGB în CMYK simplificat (doar pentru operatorul K)
    // Hummus folosește K(c, m, y, k) pentru culoare de stroke
    double c = 0.0, m = 0.0, yk = 0.0, k = 0.0;

    // Conversie simplă RGB → CMYK (nu perfectă, dar funcțională)
    double r = color.r, g = color.g, b = color.b;
    k = 1.0 - std::max<double>({ r, g, b });
    if (k < 1.0) {
        c = (1.0 - r - k) / (1.0 - k);
        m = (1.0 - g - k) / (1.0 - k);
        yk = (1.0 - b - k) / (1.0 - k);
    }

    // Desenăm linia
    m_currentPageContext->q();           // Salvează starea grafică
    m_currentPageContext->w(thickness); // Grosimea liniei
    m_currentPageContext->K(c, m, yk, k); // Culoarea liniei

    m_currentPageContext->m(x1, y1);     // Punct de start
    m_currentPageContext->l(x2, y2);     // Punct final
    m_currentPageContext->s();           // Stroke path

    m_currentPageContext->Q();           // Restaurează starea grafică
}


void PdfWriterWrapper::addRectangle(double x, double y, double width, double height, const ColorRgb& fillColor, double borderWidth, const ColorRgb& borderColor) {
        addRectangleSolidBorder(x, y, width, height, fillColor, borderWidth, borderColor);
}

void PdfWriterWrapper::addRectangleSolidBorder(double x, double y, double width, double height, const ColorRgb& fillColor, double borderWidth, const ColorRgb& borderColor) {
    if (!m_currentPageContext) return;

    // 1. DESENAREA UMPLERII (Fill)
    // !!! CORECȚIE CRITICĂ: Săriți peste Umplere dacă detectați valoarea specială NO FILL.
    // Presupunând că R, G, B normale sunt de la 0.0 la 1.0.
    bool shouldFill = (fillColor.r >= 0.0 || fillColor.g >= 0.0 || fillColor.b >= 0.0);
  
    if (shouldFill) {
        // În loc de DrawRectangle cu fillOptions, folosim operatori direct

        m_currentPageContext->q(); // Salvează starea grafică

        // Setează culoarea de umplere (cu valori R, G, B între 0.0 și 1.0)
        m_currentPageContext->rg(fillColor.r, fillColor.g, fillColor.b);

        // Definește dreptunghiul (re(x, y, lățime, înălțime))
        m_currentPageContext->re(x, y, width, height);

        // Umple dreptunghiul (f)
        m_currentPageContext->f();

        m_currentPageContext->Q(); // Restaurează starea grafică
    }

    // 2. DESENAREA CHENARULUI (Stroke)
    if (borderWidth > 0.0) {
        // Calculăm culoarea chenarului (Border)
        long borderValue = (long)(borderColor.r * 255) << 16 | 
            (long)(borderColor.g * 255) << 8 | 
            (long)(borderColor.b * 255);

        // Creăm opțiunile pentru chenar (Stroke)
        AbstractContentContext::GraphicOptions strokeOptions(
            AbstractContentContext::eStroke, // Tipul de desenare: Contur
            AbstractContentContext::eRGB,
            borderValue,
            borderWidth // Setăm lățimea liniei
        );
        
        // Aplicăm chenarul (folosim aceleași coordonate)
        m_currentPageContext->DrawRectangle(x, y, width, height, strokeOptions);
    }
}


void PdfWriterWrapper::addRectangleDashedBorder(double x, double y, double width, double height, const ColorRgb& fillColor, double borderWidth, const ColorRgb& borderColor) {
    if (!m_currentPageContext) return;

    // Calculăm ajustarea necesară pentru a desena fundalul ÎN INTERIORUL bordurii.
    // Fundalul se desenează pe o suprafață mai mică cu 2 * borderWidth pe ambele axe.
    double bg_adjustment = borderWidth;
    double bg_x = x + bg_adjustment;
    double bg_y = y + bg_adjustment; // Y este coordonata de jos (Bottom-Up)
    double bg_width = width - 2.0 * bg_adjustment;
    double bg_height = height - 2.0 * bg_adjustment;

    // 1. DESENAREA UMPLERII (Fill)
    bool shouldFill = (fillColor.r >= 0.0 || fillColor.g >= 0.0 || fillColor.b >= 0.0);

    // Asigurăm că nu desenăm umplere negativă
    if (shouldFill && bg_width > 0.0 && bg_height > 0.0) {

        m_currentPageContext->q();
        m_currentPageContext->rg(fillColor.r, fillColor.g, fillColor.b);

        // 🎯 COORDONATE CORECTATE PENTRU FUNDAL (Padding Box)
        m_currentPageContext->re(bg_x, bg_y, bg_width, bg_height);

        m_currentPageContext->f();
        m_currentPageContext->Q();
    }

    // 2. DESENAREA CHENARULUI (Stroke)
    // Chenarul se desenează pe coordonatele Box-ului.
    if (borderWidth > 0.0) {
        m_currentPageContext->q();

        m_currentPageContext->RG(borderColor.r, borderColor.g, borderColor.b);
        m_currentPageContext->w(borderWidth);

        double dashPattern[] = { 3.0, 2.0 };
        double dashPhase = 0.0;
        m_currentPageContext->d(dashPattern, 2, dashPhase);

        // 🎯 COORDONATE CORECTE PENTRU BORDURĂ (Border Box)
        m_currentPageContext->re(x, y, width, height);

        m_currentPageContext->S();
        m_currentPageContext->Q();
    }
}

double PdfWriterWrapper::measureText(const std::wstring& text, const double font_size) {
    if (!m_currentPageContext) return false;

    if (!m_currentFont || text.empty()) {
        LOG_DEBUG(L"[MEASURE] Fontul lipsește sau textul este gol. Lățime: 0.0");
        return 0.0;
    }

    // 1. Conversia în UTF-8 (necesară pentru funcția subiacentă)
    std::string utf8Text = wstring_to_utf8(text);

    double actualWidth = m_currentFont->CalculateTextAdvance(
        utf8Text, // Presupunem că acceptă std::string sau C-string
        font_size // Dimensiunea fontului în puncte (pt)
    );


    return actualWidth;
}

// PdfWriterWrapper.cpp

double PdfWriterWrapper::measureTextWidth(const std::wstring& text, const Style& style) {

    // 0. GESTIUNE FONT (Pre-măsurare)

    // ⭐ CORECȚIE: Trebuie să obții fontul bazat pe 'style.fontFamily' și 'style.fontWeight'.
    // Presupunem că ai o metodă (similară cu ceea ce ai pentru m_currentFont) care selectează fontul:
    PDFUsedFont* currentFont = getFontForStyle(style); // O funcție ipotetică internă

    if (!currentFont || text.empty()) {
        LOG_DEBUG(L"[MEASURE] Fontul lipsește sau textul este gol. Lățime: 0.0");
        return 0.0;
    }

    // 1. Măsurarea Lățimii de Bază (Fără Spațieri CSS)

    // Conversia în UTF-8
    std::string utf8Text = wstring_to_utf8(text);

    double baseWidth = currentFont->CalculateTextAdvance(
        utf8Text,
        style.fontSize // Folosește dimensiunea fontului din Style
    );


    // 2. APLICAREA SPAȚIERILOR CSS

    double additionalSpacing = 0.0;

    // A. Aplică letter-spacing (Spațierea dintre caractere)
    // Se aplică pentru fiecare caracter, mai puțin ultimul.
    if (style.letterSpacing > 0.0 && text.length() > 0) {
        additionalSpacing += style.letterSpacing * (text.length() - 1);
    }

    // B. Aplică word-spacing (Spațierea dintre cuvinte)
    // Acesta se aplică DOAR dacă token-ul este un spațiu (" ").
    if (text == L" ") {
        // În CSS, word-spacing se adaugă la lățimea implicită a spațiului.
        // Presupunem că 'baseWidth' este lățimea implicită a spațiului.
        additionalSpacing += style.wordSpacing;
    }


    // 3. REZULTAT FINAL

    double finalWidth = baseWidth + additionalSpacing;

    // Logarea este utilă pentru debug!
    LOG_DEBUG(L"[MEASURE] Text: '" + text + L"', Lățime Finală: " + std::to_wstring(finalWidth) +
        L", Base: " + std::to_wstring(baseWidth) + L", Aditional: " + std::to_wstring(additionalSpacing));

    return finalWidth;
}


/**
 * Salvează starea grafică curentă în stiva PDF (operatorul 'q').
 * Aceasta izolează setările curente (font, culoare, transformări, clipping)
 * de operațiunile viitoare.
 */
void PdfWriterWrapper::saveGraphicState() {
    if (m_currentPageContext) {
        // q() este echivalentul din PageContentContext pentru salvarea stării grafice
        m_currentPageContext->q();
    }
}

/**
 * Restaurează starea grafică anterioară din stiva PDF (operatorul 'Q').
 * Anulează modificările făcute după cel mai recent apel la q().
 */
void PdfWriterWrapper::restoreGraphicState() {
    if (m_currentPageContext) {
        // Q() este echivalentul din PageContentContext pentru restaurarea stării grafice
        m_currentPageContext->Q();
    }
}



void PdfWriterWrapper::setFillColor(const ColorRgb& color) {
    if (!m_currentPageContext) return;

    // PDFHummus RG: Setează culoarea non-stroking (fill) în spațiul RGB.
    // Valorile [0.0, 1.0] sunt folosite direct.
    m_currentPageContext->RG(color.r, color.g, color.b);
}

// Implementare drawFilledRectangle
void PdfWriterWrapper::drawFilledRectangle(double x, double y, double width, double height, const ColorRgb& color) {
    if (!m_currentPageContext) return;

    // 1. Salvăm starea grafică (q)
    m_currentPageContext->q();

    // 2. Setează Culoarea de Umplere (IZOLATĂ)
    // PDFHummus RG: Setează culoarea non-stroking (fill) în spațiul RGB.
    m_currentPageContext->RG(color.r, color.g, color.b);

    // 3. Definirea căii: Adaugă un dreptunghi.
    m_currentPageContext->re(x, y, width, height);

    // 4. Umple calea (f)
    m_currentPageContext->f();

    // 5. Restaurăm starea grafică (Q) - garantând că starea este curată pentru addText
    m_currentPageContext->Q();
}




inline std::string ObjectIDTypeToString(ObjectIDType inObjectID) {
    return "Im" + std::to_string(inObjectID);
}


std::string readFileToBinaryString(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        
        LOG_ERROR(L"readFileToBinaryString:EROARE LA DESCHIDEREA" + str_to_wstr(filename));
        return ""; // Eroare la deschidere
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

bool PdfWriterWrapper::addImageFromFile(
    const std::string& file_path_utf8,
    double width_pt,
    double height_pt,
    double x_pt,
    double y_pt
) {
    // 1. Citește întregul fișier în memorie (ca string binar)
    std::string image_data_binary = readFileToBinaryString(file_path_utf8);

    if (image_data_binary.empty()) {
        LOG_ERROR(L"[PDF Wrapper] Nu s-a putut citi fișierul în memorie: " + str_to_wstr(file_path_utf8));
        return false;
    }

    // 2. Apelați funcția standard de adăugare imagine (care detectează JPG/PNG
    // și folosește stream-uri din memorie acolo unde este posibil).
    // NOTĂ: Dacă știți că este mereu JPEG, puteți apela direct addJpegImageFromBinary.
    // Presupunând că addImage detectează formatul:
    bool success = addImage(image_data_binary, width_pt, height_pt, x_pt, y_pt);

    if (success) {
        LOG_INFO(L"Imagine externă %S inserată cu succes prin memorie." + str_to_wstr(file_path_utf8));
    }
    return success;
}

bool PdfWriterWrapper::addJpegImageFromBinary(
    const std::string& image_data_binary,
    double width_pt,
    double height_pt,
    double x_pt,
    double y_pt
) {
    if (!m_currentPageContext) return false;

    // Log 1: Dimensiunea datelor binare
    std::wstring log1 = L"[JPG Stream] Incepe procesarea imaginii. Dimensiune date binare: " + std::to_wstring(image_data_binary.size());
    LOG_DEBUG(log1);

    // 1. Creează o instanță a adaptorului de stream (StringReader)
    StringReader jpgReader(image_data_binary);

    // Log 2: Lungimea totală a stream-ului
    std::wstring log2 = L"[JPG Stream] StringReader creat. Lungimea totala raportata: " + std::to_wstring(jpgReader.GetLength());
    LOG_DEBUG(log2);

    // 2. Creează obiectul imagine direct din stream
    PDFImageXObject* imageObject = m_writer->CreateImageXObjectFromJPGStream(&jpgReader);

    // Log 3 & 4 & 5: Starea cititorului după ce a fost folosit
    LOG_DEBUG(L"[JPG Stream] Apel CreateImageXObjectFromJPGStream finalizat.");

    std::wstring log4 = L"[JPG Stream] Poziția finală a StringReader: " + std::to_wstring(jpgReader.GetCurrentPosition());
    LOG_DEBUG(log4);

    std::wstring log5 = L"[JPG Stream] StringReader IsGood: " + std::wstring(jpgReader.IsGood() ? L"DA" : L"NU") +
        L", NotEnded: " + std::wstring(jpgReader.NotEnded() ? L"DA" : L"NU");
    LOG_DEBUG(log5);

    if (!imageObject) {
        // Log 6: Eroare la creare
        LOG_ERROR(L"[PDF Wrapper] Eroare la încărcarea imaginii JPEG din stream de memorie. Obiectul este NULL.");
        return false;
    }

    // Generează un nume simbolic unic
    std::string imageName = "Im_" + std::to_string(imageCounter++);

    // Log 7: Numele resursei
    std::wstring log7 = L"[JPG Stream] Imaginea a fost creata. Nume resursa: " + str_to_wstr(imageName);
    LOG_DEBUG(log7);

    // 3. Adaugă imaginea în dicționarul de resurse
    m_currentPageContext->GetAssociatedPage()->GetResourcesDictionary().AddImageXObjectMapping(
        imageObject,
        imageName
    );

    // 4. Desenează imaginea
    LOG_DEBUG(L"[JPG Stream] Incepe desenarea imaginii.");
    m_currentPageContext->q();
    m_currentPageContext->cm(width_pt, 0, 0, height_pt, x_pt, y_pt);
    m_currentPageContext->Do(imageName);
    m_currentPageContext->Q();
    LOG_DEBUG(L"[JPG Stream] Desenare finalizata.");

    return true;
}

bool PdfWriterWrapper::addPngImageFromBinary(
    const std::string& image_data_binary,
    double width_pt, // Lățimea finală dorită
    double height_pt, // Înălțimea finală dorită
    double x_pt,
    double y_pt
) {
    if (!m_currentPageContext) return false;

    // 1. Definim calea fișierului temporar
    // Asigurați-vă că 'm_imageCounter' este membru al clasei și este disponibil.
    std::string temp_file_path = "temp_png_" + std::to_string(imageCounter) + ".png";

    // 2. Scrie datele binare PNG decodate în fișierul temporar
    std::ofstream ofs(temp_file_path, std::ios::out | std::ios::binary);
    if (!ofs) {
        LOG_ERROR(L"[PDF Wrapper] Nu s-a putut crea fișierul temporar PNG.");
        return false;
    }
    ofs.write(image_data_binary.data(), image_data_binary.size());
    ofs.close();

    // 3. CORECȚIE C2039: Creează un Form XObject din fișier (Hummus)
    PDFFormXObject* image_form = m_writer->CreateFormXObjectFromPNGFile(temp_file_path.c_str());

    // 4. Curățenie: Șterge fișierul temporar imediat
    std::remove(temp_file_path.c_str());

    if (!image_form) {
        LOG_ERROR(L"[PDF Wrapper] Eroare la încărcarea imaginii PNG din fișier temporar.");
        return false;
    }

    // 5. Desenează imaginea
    std::string imageName = "Im_" + std::to_string(imageCounter++);

    // CORECȚIE: Mapează Form XObject, nu Image XObject
    m_currentPageContext->GetAssociatedPage()->GetResourcesDictionary().AddFormXObjectMapping(
        image_form->GetObjectID(), // <-- Aici este corecția
        imageName
    );

    m_currentPageContext->q();
    // Aplică transformarea CM (Coordonate Matrix) folosind dimensiunile finale dorite
    m_currentPageContext->cm(width_pt, 0, 0, height_pt, x_pt, y_pt);
    m_currentPageContext->Do(imageName);
    m_currentPageContext->Q();

    LOG_INFO(L"Imagine Base64 (PNG) inserată cu succes prin fișier temporar.");
    return true;
}

bool PdfWriterWrapper::addImage(
    const std::string& image_data_binary,
    double width_pt,
    double height_pt,
    double x_pt,
    double y_pt
) {
    if (!m_currentPageContext) {
        LOG_ERROR(L"[PDF Wrapper] Nu este inițializat contextul paginii.");
        return false;
    }

    // Detectează formatul imaginii
    if (image_data_binary.size() > 2 &&
        static_cast<unsigned char>(image_data_binary[0]) == 0xFF &&
        static_cast<unsigned char>(image_data_binary[1]) == 0xD8) {
        // JPEG
        LOG_DEBUG(L"[PDF Wrapper] AM GASIT Formatul imaginii JPG.");
        return addJpegImageFromBinary(image_data_binary, width_pt, height_pt, x_pt, y_pt);
    }

    if (image_data_binary.size() > 8 &&
        image_data_binary.substr(0, 8) == "\x89PNG\r\n\x1a\n") {
        // PNG
        LOG_DEBUG(L"[PDF Wrapper] AM GASIT Formatul imaginii PNG.");
        return addPngImageFromBinary(image_data_binary, width_pt, height_pt, x_pt, y_pt);
    }

    LOG_ERROR(L"[PDF Wrapper] Formatul imaginii nu este recunoscut (JPG/PNG).");
    return false;
}

void PdfWriterWrapper::beginPage(const Page& page) {

    // 1. Finalizează pagina anterioară, dacă există
    if (m_currentPageContext) {
        m_writer->EndPageContentContext(m_currentPageContext);
        m_currentPageContext = nullptr;
    }
    if (m_currentPage) {
        // Eliberează memoria paginii anterioare
        m_writer->WritePageAndRelease(m_currentPage);
        m_currentPage = nullptr;
    }

    // 2. Creează pagina nouă (alocare memorie)
    m_currentPage = new PDFPage();

    // 3. Setează dimensiunea MediaBox (Dimensiunea totală a paginii)
    // ⭐ CORECTIE: Folosim Getters pentru a accesa dimensiunile paginii.
    m_currentPage->SetMediaBox(PDFRectangle(0, 0, page.getWidth(), page.getHeight()));

    // 4. Inițiază contextul de conținut
    m_currentPageContext = m_writer->StartPageContentContext(m_currentPage);

    // ⭐ Sfat: Ar trebui să setezi și o zonă de tăiere (CropBox/TrimBox) dacă ai nevoie de margini 
    // care sunt *dincolo* de zona de conținut, dar pentru simplitate, MediaBox este suficient.

    LOG_DEBUG(L"[PDF Wrapper] Pagina nouă inițializată. Dimensiuni: " +
        std::to_wstring(page.getWidth()) + L"x" + std::to_wstring(page.getHeight()));
}


void PdfWriterWrapper::registerFont(const pdfFontKey& key, PDFUsedFont* font) {

    if (!font) {
        LOG_WARNING(L"[FONT REGISTRATION] Tentativă de a înregistra un font NULL. Ignorat.");
        return;
    }

    // Converteste cheia in format string pentru logare
    std::wstring key_str = key.family + L"-" + key.weight + L"-" + key.style;

    // Verifică dacă fontul este deja înregistrat
    if (m_fontCache.count(key)) {
        LOG_WARNING(L"[FONT REGISTRATION] Fontul '" + key_str + L"' este deja înregistrat. Ignor înregistrarea duplicat.");
        // Opțional: Eliberezi fontul primit dacă ești sigur că nu este folosit altundeva.
        return;
    }

    // Înregistrează fontul în cache
    m_fontCache[key] = font;

    LOG_INFO(L"[FONT REGISTRATION] Font înregistrat cu succes: " + key_str);

    // ⭐ Sfat: Setează fontul de bază implicit la prima înregistrare, dacă e cazul.
    // if (key.family == L"Arial" && key.weight == L"normal" && key.style == L"normal") {
    //     m_defaultFont = font;
    // }
}

PDFUsedFont* PdfWriterWrapper::getFontForStyle(const Style& style) {
    if (m_fontCache.empty() && !m_defaultFont) {
        // Dacă nu avem fonturi deloc, nu încercăm căutări repetate în map
        return nullptr;
    }
    // -----------------------------------------------------------------
    // 1. PRE-PROCESAREA FONT FAMILY
    // Extrage doar primul font din lista (e.g., 'Arial, sans-serif' devine 'Arial')
    // -----------------------------------------------------------------
    std::wstring requestedFamily = style.fontFamily;
    size_t commaPos = requestedFamily.find(L',');

    if (commaPos != std::wstring::npos) {
        // Taie la prima virgulă și elimină spațiile albe de la sfârșit
        requestedFamily = requestedFamily.substr(0, commaPos);
        // Poți adăuga și o funcție de trim (eliminare spații albe) aici
        // pentru a elimina spațiile de la sfârșitul lui 'Arial '.

        // De exemplu, un simplu loop sau o funcție trim():
        while (!requestedFamily.empty() && requestedFamily.back() == L' ') {
            requestedFamily.pop_back();
        }
    }

    // Asigură-te că valoarea `style.fontFamily` folosită în LOG și cheie este cea curată
    const std::wstring& cleanFamily = requestedFamily;

    //LOG_INFO(L"[FONT] Începe căutarea fontului pentru stil: "
    //    + cleanFamily + L", " // Folosește variabila curată aici
    //    + style.fontWeight + L", "
    //    + style.fontStyle);

    // 2. Creează cheia exactă (prima încercare)
    pdfFontKey exactKey = {
        cleanFamily, // FOLOSEȘTE FONTUL CURAT
        style.fontWeight,
        style.fontStyle
    };

    std::wstring exactKeyStr = exactKey.family + L"-" + exactKey.weight + L"-" + exactKey.style;
    //LOG_DEBUG(L"[FONT] Căutare font exact: " + exactKeyStr);

    // 3. Încearcă să găsești fontul exact
    if (m_fontCache.count(exactKey)) {
        //LOG_INFO(L"[FONT] Font exact găsit: " + exactKeyStr);
        return m_fontCache.at(exactKey);
    }
    else {
        LOG_WARNING(L"[FONT] Fontul exact NU a fost găsit: " + exactKeyStr);
    }

    // --- 4. LOGICA FALLBACK (Acum folosește `cleanFamily` peste tot) ---

    // A. Fallback #1: Ignoră Italic/Oblique
    if (style.fontStyle != L"normal") {
        pdfFontKey fallbackKey1 = {
            cleanFamily, // FOLOSEȘTE FONTUL CURAT
            style.fontWeight,
            L"normal"
        };
        std::wstring fallbackKey1Str = fallbackKey1.family + L"-" + fallbackKey1.weight + L"-" + fallbackKey1.style;
        LOG_DEBUG(L"[FONT] Fallback #1: Căutare fără italic: " + fallbackKey1Str);

        if (m_fontCache.count(fallbackKey1)) {
            LOG_INFO(L"[FONT] Fallback #1 reușit: " + fallbackKey1Str);
            return m_fontCache.at(fallbackKey1);
        }
        else {
            LOG_WARNING(L"[FONT] Fallback #1 eșuat: " + fallbackKey1Str);
        }
    }

    // B. Fallback #2: Ignoră Bold
    if (style.fontWeight != L"normal" && style.fontWeight != L"400") {
        pdfFontKey fallbackKey2 = {
            cleanFamily, // FOLOSEȘTE FONTUL CURAT
            L"normal",
            style.fontStyle
        };
        std::wstring fallbackKey2Str = fallbackKey2.family + L"-" + fallbackKey2.weight + L"-" + fallbackKey2.style;
        LOG_DEBUG(L"[FONT] Fallback #2: Căutare fără bold: " + fallbackKey2Str);

        if (m_fontCache.count(fallbackKey2)) {
            LOG_INFO(L"[FONT] Fallback #2 reușit: " + fallbackKey2Str);
            return m_fontCache.at(fallbackKey2);
        }
        else {
            LOG_WARNING(L"[FONT] Fallback #2 eșuat: " + fallbackKey2Str);
        }
    }

    // C. Fallback #3: Fontul de bază
    pdfFontKey baseKey = {
        cleanFamily, // FOLOSEȘTE FONTUL CURAT
        L"normal",
        L"normal"
    };
    std::wstring baseKeyStr = baseKey.family + L"-" + baseKey.weight + L"-" + baseKey.style;
    LOG_DEBUG(L"[FONT] Fallback #3: Căutare font de bază: " + baseKeyStr);

    if (m_fontCache.count(baseKey)) {
        LOG_INFO(L"[FONT] Fallback #3 reușit: " + baseKeyStr);
        return m_fontCache.at(baseKey);
    }
    else {
        LOG_WARNING(L"[FONT] Fallback #3 eșuat: " + baseKeyStr);
    }

    // 5. Eșec total – folosim fontul implicit
    if (m_defaultFont) {
        LOG_WARNING(L"[FONT] Se folosește fontul implicit de rezervă.");
        return m_defaultFont;
    }

    LOG_ERROR(L"[FONT] Eșec total: niciun font disponibil. Sistemul de fonturi este defect.");
    return m_defaultFont; // poate fi nullptr
}



void PdfWriterWrapper::renderText(const std::wstring& text, double x, double y, const Style& style) {

    if (text.empty()) {
        return;
    }

    // 1. SELECTAREA FONTULUI ȘI APLICAREA
    // Asigură-te că fontul curent (m_currentFont) este setat CORECT,
    // deoarece addText se bazează pe m_currentFont.

    PDFUsedFont* font = getFontForStyle(style);
    if (!font) {
        LOG_WARNING(L"[RENDER] Font inexistent. Randarea textului ignorată.");
        return;
    }

    // ⭐ CORECȚIE CRITICĂ: Trebuie să actualizezi m_currentFont înainte de addText.
    // Presupunând că ai refactorizat setFont (sau ai creat o metodă internă)
    // pentru a seta m_currentFont = font.
    // m_currentFont = font; 

    // Dacă ai o funcție setFont(font, size), o apelezi aici:
    // setFont(font, style.fontSize); // Nu am definit setFont, dar presupunem că setează m_currentFont.

    // 2. RANDAREA TEXTULUI EFECTIV

    // Acum apelăm funcția ta existentă, care știe să folosească m_currentFont, m_currentPageContext etc.
    // P.S. Mutarea m_currentFont = font; la începutul acestei funcții ar fi cea mai simplă soluție.

    // Așadar, presupunând că: m_currentFont = font; s-a executat înainte de acest apel.
    addText(x, y, text, style.fontSize, style.textColor);

    // ⭐ ATENȚIE: Spațierea (letter-spacing/word-spacing) nu este tratată în addText.
    // Dacă ai nevoie de spațiere non-zero, va trebui să modifici addText
    // sau să desenezi token-urile caracter cu caracter/cuvânt cu cuvânt (mult mai complex).
}



void PdfWriterWrapper::renderBox(
    const RenderingContext& context,
    double box_x,              // X-ul de start (stânga)
    double box_y,   // Y-ul de jos (în sistemul PDF, Y=0 e jos)
    double width,               // Lățimea totală a box-ului
    double height) {            // Înălțimea totală a box-ului

    // Obține doar stilurile necesare pentru desen
    const BoxModel& box = context.style.boxModel;
    const Style& style = context.style;

    std::wstring tagName = context.current_xhtml_element->getTagName();
    std::wstring tagContent = context.current_xhtml_element->getTagContent();

    // 1. APELUL REAL CĂTRE WRAPPER (Randare Box)
    ColorRgb fillColor = style.backgroundColor;
    ColorRgb borderColor = style.borderColor;
    double borderWidth = box.borderTopWidth;

    // Y-ul de sus în sistemul PDF (necesar pentru log)
    //double pdf_y_top_coordinate = pdf_y_bottom_coordinate + total_height;
    //double box_x_end = box_x_layout + total_width;


    // =========================================================================
    // LOG DETALIAT NOU
    // =========================================================================
    LOG_DEBUG(L"Randare <"+ tagName +L"> Box: X_Start:" + to_wstring<double>(box_x) +
        L", Y_Start:" + to_wstring<double>(box_y) +
        L", Width:" + to_wstring<double>(width) + L"; HEIGHT=" + to_wstring<double>(height) +
        L", FINAL_X=" + to_wstring<double>(box_x + width) + L", FINAL_Y=" + to_wstring<double>(box_y + height)
    );
    // =========================================================================

    addRectangle(
        box_x,
        box_y,
        width,
        height,
        fillColor,
        borderWidth,
        borderColor
    );


    // 2. RANDAREA NUMELUI TAG-ULUI (Pentru Debug)
    double tag_x = box_x + 2. * box.borderLeftWidth;

    // Y-ul textului. Se calculează din Y_Top (PDF) minus înălțimea fontului.
    double tag_y = box_y + height - 8.0;

    AbstractContentContext::TextOptions tagTextOptions(
        m_defaultFont,
        8.0, // dimensiune font mică
        AbstractContentContext::eRGB,
        0x000000
    );

    // Scrie textul
    m_currentPageContext->WriteText(tag_x, tag_y, wstring_to_utf8(tagName + L":" + tagContent), tagTextOptions);
}


void PdfWriterWrapper::renderLineBuffer(const RenderingContext& context,
    const std::vector<LineToken>& lineBuffer,
    double offset_x,
    double cursor_y) {

    if (!m_currentPageContext || !m_currentFont || lineBuffer.empty()) return;

    double PAGE_HEIGHT = 841.890000;
    double pdf_y_coordinate = PAGE_HEIGHT + cursor_y; // Inversarea Y

    //double start_x = context.current_line_start_x + offset_x;
    double start_x = context.metrics.x_content_start + offset_x;
    const Style& style = context.style;

    // Convertim culoarea în format 0xRRGGBB
    long colorValue = (long)(style.textColor.r * 255) << 16 |
        (long)(style.textColor.g * 255) << 8 |
        (long)(style.textColor.b * 255);

    // Setăm opțiunile de text
    AbstractContentContext::TextOptions textOptions(
        m_currentFont,
        style.fontSize,
        AbstractContentContext::eRGB,
        colorValue
    );

    // Poziția inițială
    double current_x = start_x;

    for (const auto& token : lineBuffer) {
        std::string utf8_text = wstring_to_utf8(token.text);

        // Scriem fiecare token la poziția curentă
        m_currentPageContext->WriteText(current_x, cursor_y, utf8_text, textOptions);

        // Avansăm X-ul (presupunem că ai o funcție de măsurare a lățimii textului)
        //current_x += measureTextWidth(utf8_text, style.fontSize);
    }

    LOG_DEBUG(L"[PDF RENDER TEXT] Textul randat: '" + lineBuffer.front().text + L"' la Y: " + std::to_wstring(cursor_y));
}


void PdfWriterWrapper::writeDebugTextByPassFlow(double x, double y_pdf, const std::wstring& text) {

    // ⭐ ATENȚIE: Trebuie să ai o instanță validă a contextului paginii!
    if (!m_currentPageContext) {
        LOG_ERROR(L"[DEBUG TEXT] Eroare: Contextul paginii nu este inițializat.");
        return;
    }

    // Convertirea textului din wstring (UTF-16) în formatul cerut de API (ex: UTF-8)
    // Presupunem că ai o funcție de conversie, ex: WString_to_UTF8(text)
    std::string utf8_text = wstring_to_utf8(text);

    // Opțiuni de stil pentru textul de debug
    // (Ar trebui să le pasezi, dar le setăm aici simplu)


    AbstractContentContext::TextOptions textOptions(
        m_currentFont,
        12,
        AbstractContentContext::eRGB,
        0x000000
    );

    // Setează un font mic (ex: 6 puncte) și o culoare contrastantă (ex: negru)
    // NOTE: Presupunem că ai o modalitate de a obține un Font (ex: din font cache)
    // Poate fi necesar să setezi FontName și FontSize
    // debugOptions.setFont(m_fontCache->getFont(L"Arial", 6.0)); // Dacă ai cache
    // Setează o culoare pentru text
//    debugOptions.setColor(ColorRgb{ 0.0, 0.0, 0.0, 1.0 }); // Negru

    // 1. Începe o secțiune de text (dacă API-ul o cere)
    // m_currentPageContext->BeginText(); // (Dacă este necesar)

    // 2. Scrie textul la coordonatele date (coordonatele Y sunt deja inversate PDF)
    m_currentPageContext->WriteText(x, y_pdf, utf8_text, textOptions);

    // 3. Terminăm secțiunea de text
    // m_currentPageContext->EndText(); // (Dacă este necesar)

    LOG_DEBUG(L"[DEBUG TEXT RENDER] Tag: <" + text + L"> scris la X: " + std::to_wstring(x) + L", Y_PDF: " + std::to_wstring(y_pdf));
}

/*
void PdfWriterWrapper::loadAllFonts() {
    try {
        PDFUsedFont* font = m_writer->GetFontForFile(m_fontFamilies["Arial"].Regular.c_str());
        if (font) {
            // APLICAȚIE: Setează fontul implicit și înregistrează în cache
            m_defaultFont = font;
            registerFont({ L"Arial", L"normal", L"normal" }, font);

        }

        // Încărcare Bold
        font = m_writer->GetFontForFile(m_fontFamilies["Arial"].Bold.c_str());
        if (font) {
            registerFont({ L"Arial", L"bold", L"normal" }, font);
        }

        // Încărcare Italic
        font = m_writer->GetFontForFile(m_fontFamilies["Arial"].Italic.c_str());
        if (font) {
            registerFont({ L"Arial", L"normal", L"italic" }, font);
        }

        // Încărcare Bold-Italic
        font = m_writer->GetFontForFile(m_fontFamilies["Arial"].BoldItalic.c_str());
        if (font) {
            registerFont({ L"Arial", L"bold", L"italic" }, font);
        }

        //m_defaultFont = m_writer.GetFontForFile(m_fontFamilies["Arial"].Regular.c_str());
        m_defaultFont = m_writer->GetFontForFile("C:\\Windows\\Fonts\\Arial.ttf");
        if (!m_defaultFont) {
            LOG_ERROR(L"Eroare: Fontul de fallback nu a fost găsit la adresa specificată.");
        }
        else {
            LOG_INFO(L"Fontul m_defaultFont a fost incarcat din C:\\Windows\\Fonts\\Arial.ttf.");
        }

    }
    catch (const std::exception& e) {
        LOG_ERROR(L"Excepție la inițializarea fonturilor: " + str_to_wstr(e.what()));
    }



    for (const auto& [familyName, paths] : m_fontFamilies) {
        try {
            // Regular
            PDFUsedFont* font = m_writer->GetFontForFile(paths.Regular.c_str());
            if (font) {
                registerFont({ str_to_wstr(familyName), L"normal", L"normal" }, font);
            }

            // Bold
            font = m_writer->GetFontForFile(paths.Bold.c_str());
            if (font) {
                registerFont({ str_to_wstr(familyName), L"bold", L"normal" }, font);
            }

            // Italic
            font = m_writer->GetFontForFile(paths.Italic.c_str());
            if (font) {
                registerFont({ str_to_wstr(familyName), L"normal", L"italic" }, font);
            }

            // BoldItalic
            font = m_writer->GetFontForFile(paths.BoldItalic.c_str());
            if (font) {
                registerFont({ str_to_wstr(familyName), L"bold", L"italic" }, font);
            }

            LOG_INFO(L"[FONT LOAD] Fonturile pentru familia '" + str_to_wstr(familyName) + L"' au fost încărcate.");
        }
        catch (const std::exception& e) {
            LOG_ERROR(L"[FONT LOAD] Eroare la încărcarea fonturilor pentru '" + str_to_wstr(familyName) + L"': " + str_to_wstr(e.what()));
        }
    }
}
*/

void PdfWriterWrapper::loadAllFonts() {
    if (!m_writer) return;

    // Helper lambda pentru a căuta fișierul de font în folderul local sau în Windows Fonts
    auto findFontFile = [](const std::string& relativePath) -> std::string {
        if (std::filesystem::exists(relativePath)) {
            return relativePath;
        }
        // Încercăm în C:\Windows\Fonts dacă e cale relativă "Fonts\..."
        std::filesystem::path p(relativePath);
        std::string winPath = "C:\\Windows\\Fonts\\" + p.filename().string();
        if (std::filesystem::exists(winPath)) {
            return winPath;
        }
        return "";
    };

    for (const auto& [familyName, paths] : m_fontFamilies) {
        try {
            std::wstring wFamily = str_to_wstr(familyName);

            // Regular
            std::string regPath = findFontFile(paths.Regular);
            if (!regPath.empty()) {
                PDFUsedFont* font = m_writer->GetFontForFile(regPath.c_str());
                if (font) {
                    registerFont({ wFamily, L"normal", L"normal" }, font);
                    if (!m_defaultFont) m_defaultFont = font; // Setează-l ca default dacă nu avem
                }
            }

            // Bold
            std::string boldPath = findFontFile(paths.Bold);
            if (!boldPath.empty()) {
                PDFUsedFont* font = m_writer->GetFontForFile(boldPath.c_str());
                if (font) {
                    registerFont({ wFamily, L"bold", L"normal" }, font);
                }
            }

            // Italic
            std::string italicPath = findFontFile(paths.Italic);
            if (!italicPath.empty()) {
                PDFUsedFont* font = m_writer->GetFontForFile(italicPath.c_str());
                if (font) {
                    registerFont({ wFamily, L"normal", L"italic" }, font);
                }
            }

            // BoldItalic
            std::string biPath = findFontFile(paths.BoldItalic);
            if (!biPath.empty()) {
                PDFUsedFont* font = m_writer->GetFontForFile(biPath.c_str());
                if (font) {
                    registerFont({ wFamily, L"bold", L"italic" }, font);
                }
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR(L"[FONT LOAD] Excepție la încărcarea fonturilor pentru '" + str_to_wstr(familyName) + L"': " + str_to_wstr(e.what()));
        }
    }

    // Fallback de urgență direct pe Arial din Windows
    if (!m_defaultFont) {
        std::string sysArial = findFontFile("arial.ttf");
        if (!sysArial.empty()) {
            m_defaultFont = m_writer->GetFontForFile(sysArial.c_str());
            if (m_defaultFont) {
                registerFont({ L"Arial", L"normal", L"normal" }, m_defaultFont);
                LOG_INFO(L"[FONT LOAD] Fallback reușit pe Arial din sistem.");
            }
        }
    }
}

void PdfWriterWrapper::logFontCache() const {
    LOG_INFO(L"[FONT CACHE] Fonturi înregistrate:");
    for (const auto& [key, font] : m_fontCache) {
        std::wstring key_str = key.family + L"-" + key.weight + L"-" + key.style;
        LOG_INFO(L"  • " + key_str);
    }
}



PDFUsedFont* PdfWriterWrapper::getCachedFont(const std::wstring& fontName) {
    pdfFontKey key;
    key.family = fontName;
    key.weight = L"normal"; // sau L"400"
    key.style = L"normal";

    auto it = m_fontCache.find(key);
    if (it != m_fontCache.end()) {
        return it->second;
    }

    // Dacă nu există în cache, returnăm nullptr sau putem încărca fontul aici
    return nullptr;
}

bool PdfWriterWrapper::addImageFromFile2(
    const std::string& file_path_utf8, // ex: "imgs/romatsa.jpg"
    double width_pt,
    double height_pt,
    double x_pt,
    double y_pt
) {
    if (!std::filesystem::exists(file_path_utf8)) {
       LOG_ERROR(L"Image file not found: " + str_to_wstr(file_path_utf8 ));
        return false;
    }


    if (!m_currentPageContext) return false;
    try {
        // Creează opțiuni pentru imagine
        AbstractContentContext::ImageOptions opt;
        opt.transformationMethod = AbstractContentContext::eFit;
        opt.boundingBoxWidth = width_pt;
        opt.boundingBoxHeight = height_pt;
        opt.fitProportional = false; // true dacă vrei să păstrezi proporțiile

        // Desenează imaginea pe pagina curentă
        m_currentPageContext->DrawImage(x_pt, y_pt, file_path_utf8, opt);

        return true;
    }
    catch (...) {
        // Gestionare simplă a erorilor
        return false;
    }
}

