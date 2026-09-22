// PdfWriterWrapper.hpp

#pragma once
#include <OutputStringBufferStream.h>

#include "stringUtils.hpp"
#include <PDFWriter.h>
#include <PDFPage.h>
#include <PageContentContext.h>
#include <PDFUsedFont.h>
#include <AbstractContentContext.h>
#include "RenderingContex.hpp"
#include <string>
#include <map>
// Includeți headerele necesare pentru PDFWriter


#include "IByteReaderWithPosition.h"
#include "IByteWriterWithPosition.h"

#include <string>

struct LineToken {
    std::wstring text;       // Cuvântul sau spațiul de randat
    double width;            // Lățimea token-ului în unități PDF (pt)
    // Aici poți adăuga și alte stiluri, dacă ai randare token-cu-token (ex: font, culoare)
    Style style; 
};


class StringReader : public IByteReaderWithPosition {
private:
    const std::string& m_buffer;
    LongFilePositionType m_position;

public:
    StringReader(const std::string& buffer) : m_buffer(buffer), m_position(0) {}
    virtual ~StringReader() {}

    // Implementare moștenită din IByteReader:

    // 1. Read (Probabil din IByteReader, presupunând că returnează size_t)
    virtual size_t Read(Byte* inBuffer, size_t inBufferSize) {
        size_t bytes_to_read = std::min<size_t>((size_t)inBufferSize, (size_t)(m_buffer.size() - m_position));
        if (bytes_to_read > 0) {
            m_buffer.copy((char*)inBuffer, bytes_to_read, m_position);
            m_position += bytes_to_read;
        }
        return bytes_to_read;
    }

    // 2. Metoda IsGood() (Presupunând că este cerută)
    virtual bool IsGood() { return true; }

    // 3. Metoda NotEnded() (Presupunând că este cerută)
    virtual bool NotEnded() { return m_position < m_buffer.size(); }

    // 4. Metoda Flush() (Presupunând că este cerută)
    virtual void Flush() {}

    // 5. Metoda Close() (Presupunând că este cerută)
    virtual void Close() {}

    // 6. Metoda GetLength() (Presupunând că este cerută și este const)
    virtual LongFilePositionType GetLength() const { return m_buffer.size(); }

    // --- Implementare din IByteReaderWithPosition (NOILE CORECȚII) ---

    // 7. GetCurrentPosition() (Implementarea este similară, dar funcția trebuie să fie const)
    // ATENȚIE: Interfața dvs. NU a marcat-o ca fiind const, dar majoritatea interfețelor I/O o fac. 
    // Voi respecta interfața dvs. NE-CONST pentru a evita eroarea C2512/C2509, dar ar trebui să fie const!
    virtual LongFilePositionType GetCurrentPosition() {
        return m_position;
    }

    // 8. SetPosition (înlocuiește vechiul SetCurrentPosition)
    virtual void SetPosition(LongFilePositionType inOffsetFromStart) {
        if (inOffsetFromStart < 0 || inOffsetFromStart > m_buffer.size())
            // Deoarece funcția este 'void', nu putem returna cod de eroare,
            // așa că forțăm poziția la limită sau ignorăm.
            m_position = std::min<double>(std::max<double>(inOffsetFromStart, (LongFilePositionType)0), (LongFilePositionType)m_buffer.size());
        else
            m_position = inOffsetFromStart;
    }

    // 9. SetPositionFromEnd (înlocuiește vechiul SetCurrentPositionFromEnd)
    virtual void SetPositionFromEnd(LongFilePositionType inOffsetFromEnd) {
        SetPosition(m_buffer.size() - inOffsetFromEnd);
    }

    // 10. Skip (Implementare nouă)
    virtual void Skip(LongBufferSizeType inSkipSize) {
        SetPosition(m_position + inSkipSize);
    }
};

// Clasa custom pentru scriere PDF direct în RAM
class StringWriter : public IByteWriterWithPosition {
private:
    std::string m_buffer;
    LongFilePositionType m_position;

public:
    StringWriter() : m_position(0) {}
    virtual ~StringWriter() {}

    // 1. Implementare din IByteWriter
    virtual LongBufferSizeType Write(const Byte* inBuffer, LongBufferSizeType inBufferSize) {
        if (inBufferSize > 0 && inBuffer != nullptr) {
            if (m_position + inBufferSize > m_buffer.size()) {
                m_buffer.resize((size_t)(m_position + inBufferSize));
            }
            std::copy(inBuffer, inBuffer + inBufferSize, m_buffer.begin() + m_position);
            m_position += inBufferSize;
        }
        return inBufferSize;
    }

    // 2. Implementare din IByteWriterWithPosition
    virtual LongFilePositionType GetCurrentPosition() {
        return m_position;
    }

    virtual void SetPosition(LongFilePositionType inOffsetFromStart) {
        if (inOffsetFromStart > m_buffer.size()) {
            m_buffer.resize((size_t)inOffsetFromStart);
        }
        m_position = inOffsetFromStart;
    }

    virtual void SetPositionFromEnd(LongFilePositionType inOffsetFromEnd) {
        if (inOffsetFromEnd <= m_buffer.size()) {
            m_position = m_buffer.size() - inOffsetFromEnd;
        }
    }

    // Uneltele noastre ajutătoare
    const std::string& getString() const { return m_buffer; }

    std::vector<uint8_t> getBuffer() const {
        return std::vector<uint8_t>(m_buffer.begin(), m_buffer.end());
    }
};


struct pdfFontKey {
    // Membrii corespund direct proprietăților CSS
    std::wstring family;
    std::wstring weight; // ex: L"normal", L"bold", L"400", L"700"
    std::wstring style;  // ex: L"normal", L"italic", L"oblique"

    // Necesare pentru a folosi FontKey ca cheie într-un std::map
    // (std::wstring este comparabil, așa că această comparație în cascadă funcționează)
    bool operator<(const pdfFontKey& other) const {
        if (family != other.family) return family < other.family;
        if (weight != other.weight) return weight < other.weight;
        return style < other.style;
    }
};


class PdfWriterWrapper {
private:
    bool m_isFinalized = false; // Flag nou
    //PDFWriter m_writer;
    std::unique_ptr<PDFWriter> m_writer;

    PDFPage* m_currentPage = nullptr;
    PageContentContext* m_currentPageContext = nullptr;

    // Cache pentru fonturi (pentru a le încărca o singură dată)
    std::map<pdfFontKey, PDFUsedFont*> m_fontCache;

    // Fontul curent setat (pentru a nu reîncărca la fiecare apel addText)
    PDFUsedFont* m_currentFont = nullptr;
    PDFUsedFont* m_defaultFont = nullptr;

    std::map<std::string, FontStylePaths> m_fontFamilies;
    bool m_isBoldDesired = false;

    unsigned int imageCounter = 0;

    
    // ⭐ MEMBRII NOI PENTRU SCRIERE ÎN MEMORIE (RAM)
    std::stringbuf m_memoryBuffer;
    std::unique_ptr<StringWriter> m_memoryStream;
    bool m_isInMemoryMode = false;


public:
    PdfWriterWrapper();// = default;
    ~PdfWriterWrapper(); // Va închide documentul dacă nu a fost deja închis
    // INTERZICEREA COPIERII ȘI MUTĂRII (Soluția cea mai sigură)
    PdfWriterWrapper(const PdfWriterWrapper&) = delete;
    PdfWriterWrapper& operator=(const PdfWriterWrapper&) = delete;
    PdfWriterWrapper(PdfWriterWrapper&&) = delete;
    PdfWriterWrapper& operator=(PdfWriterWrapper&&) = delete;

    // --- Controlul Documentului ---
    bool initialize(const std::wstring& filePath, double width, double height);
    bool finalize();
    void hardReset() {
        // Aceasta distruge obiectul PDFWriter curent și eliberează toate resursele Hummus.
        m_writer.reset();
    }

    void registerFont(const pdfFontKey& key, PDFUsedFont* font);
    PDFUsedFont* getFontForStyle(const Style& style);

    // --- Controlul Paginilor ---
    void startPage(double width, double height); // Am adăugat dimensiuni pentru a fi configurabil
    void endPage();

    void beginPage(const Page& page);

    // --- Formatare ---
    // setFont acum returnează fontul, dar va fi cache-uit intern
    //void setFont(const std::string& fontName, double size, bool bold = false, bool italic = false);
    //void setTextColor(const ColorRgb& color);

    // --- Desenare ---
    void renderText(const std::wstring& text, double x, double y, const Style& style);
    void addText(double x, double y, const std::wstring& text, double fontSize, const ColorRgb& textColor, std::wstring fontFamily = L"default");
    void addTextWithSyle(double x, double y, const std::wstring& text, const Style& style);

    void addLine(double x1, double y1, double x2, double y2, double thickness, const ColorRgb& color);
    void addRectangle(double x, double y, double width, double height, const ColorRgb& fillColor, double borderWidth, const ColorRgb& borderColor);
    void addRectangleSolidBorder(double x, double y, double width, double height, const ColorRgb& fillColor, double borderWidth, const ColorRgb& borderColor);
    void addRectangleDashedBorder(double x, double y, double width, double height, const ColorRgb& fillColor, double borderWidth, const ColorRgb& borderColor);

    
    void setFillColor(const ColorRgb& color);
    
    void drawFilledRectangle(double x, double y, double width, double height, const ColorRgb& color);

    double measureText(const std::wstring& text, const double font_size);

    double measureTextWidth(
        const std::wstring& text,
        const Style& style // Noul parametru: Style complet
    );


    void saveGraphicState();
    void restoreGraphicState();

    /**
     * @brief Inserează o imagine din date binare decodate (ex: Base64) în document.
    */ 
    bool addImage(
        const std::string& image_data_binary,
        double width_pt,
        double height_pt,
        double x_pt,
        double y_pt
    );

    bool addImageFromFile(
        const std::string& file_path_utf8, // Calea: "imgs/romatsa.jpg"
        double width_pt,
        double height_pt,
        double x_pt,
        double y_pt
    );


    bool addImageFromFile2(
        const std::string& file_path_utf8, // Calea: "imgs/romatsa.jpg"
        double width_pt,
        double height_pt,
        double x_pt,
        double y_pt
    );

    bool addJpegImageFromBinary(
        const std::string& image_data_binary,
        double width_pt,
        double height_pt,
        double x_pt,
        double y_pt
    );

    bool addPngImageFromBinary(
        const std::string& image_data_binary,
        double width_pt,
        double height_pt,
        double x_pt,
        double y_pt
    );


    void renderBox( const RenderingContext& context,
            double box_x,              // X-ul de start (stânga)
            double box_y,   // Y-ul de jos (în sistemul PDF, Y=0 e jos)
            double width,               // Lățimea totală a box-ului
            double height);

    void renderLineBuffer(const RenderingContext& context,
        const std::vector<LineToken>& lineBuffer, // Presupunând această structură
        double offset_x,
        double cursor_y);

    void logFontCache() const;

  

    // ⭐ METODELE NOILE PENTRU MEMORIE
    bool initializeToMemory(double width, double height);
    std::vector<uint8_t> getMemoryPdfBuffer() const {
        if (!m_memoryStream) return {};
        return m_memoryStream->getBuffer();
    }

    std::vector<uint8_t> getBuffer() const {
        return getMemoryPdfBuffer();
    }

    std::string getMemoryData() const {
        if (m_memoryStream) {
            // PDFHummus StringWriter moștenește un buffer ale cărui date le putem returna
            return m_memoryStream->getString();
        }
        return "";
    }

private:
    PDFUsedFont* getCachedFont(const std::wstring& fontName);

    void loadAllFonts();

    void writeDebugTextByPassFlow(double x, double y_pdf, const std::wstring& text);
   
};