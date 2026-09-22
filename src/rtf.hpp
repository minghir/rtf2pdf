#pragma once
#include "page.hpp"

#include "style.hpp"
#include <vector>
#include <memory> // Necesar pentru std::unique_ptr

enum class RtfOrientation {
    Portrait,
    Landscape
};

class RtfPage {
private:
    int widthTwips = 0;      // lățimea paginii în twips
    int heightTwips = 0;     // înălțimea paginii în twips

    int marginTopTwips = 0;
    int marginBottomTwips = 0;
    int marginLeftTwips = 0;
    int marginRightTwips = 0;

    RtfOrientation orientation = RtfOrientation::Portrait;
    std::wstring sizeName = L"Custom";

public:
    // Getters
    int getWidthTwips() const { return widthTwips; }
    int getHeightTwips() const { return heightTwips; }
    int getMarginTopTwips() const { return marginTopTwips; }
    int getMarginBottomTwips() const { return marginBottomTwips; }
    int getMarginLeftTwips() const { return marginLeftTwips; }
    int getMarginRightTwips() const { return marginRightTwips; }
    RtfOrientation getOrientation() const { return orientation; }
    std::wstring getSizeName() const { return sizeName; }

    // Setters
    void setWidthTwips(int w) { widthTwips = w; }
    void setHeightTwips(int h) { heightTwips = h; }
    void setMargins(int top, int right, int bottom, int left) {
        marginTopTwips = top;
        marginRightTwips = right;
        marginBottomTwips = bottom;
        marginLeftTwips = left;
    }
    void setOrientation(RtfOrientation o) { orientation = o; }
    void setSizeName(const std::wstring& name) { sizeName = name; }
};



class RtfBlock {
public:
    virtual ~RtfBlock() = default;
};

// 1. Unitate minimală de text cu stil
struct RtfSpan {
    std::wstring text;
    Style style; // Reutilizăm structura Style
};

struct RtfParagraph : public RtfBlock {
    std::vector<RtfSpan> spans;
    Style style; // Stilul la nivel de paragraf
};


// Definim un enum pentru a reprezenta tipul de linie (simplu, dublu, etc.)
enum class RtfBorderStyle {
    None,
    Single, // \brdrs
    Double, // \brdrdb
    // ... adăugați altele dacă este necesar
};

// Structura care reține specificația unei singure borduri (stânga, dreapta, etc.)
struct BorderSpec {
    RtfBorderStyle style = RtfBorderStyle::None;
    int widthTwips = 0; // Lățimea bordurii (ex: 10)
    // Culoarea (opțional, dacă aveți o hartă de culori RTF)

    bool isSet() const { return style != RtfBorderStyle::None && widthTwips > 0; }
};



// Structura care reține toate bordurile pentru o celulă
struct CellBorders {
    BorderSpec left;
    BorderSpec right;
    BorderSpec top;
    BorderSpec bottom;
};

const int DEFAULT_CELL_PADDING = 50;

struct CellPadding {
    int leftTwips = DEFAULT_CELL_PADDING;
    //int topTwips = DEFAULT_CELL_PADDING;
    int topTwips = 0;
    int rightTwips = DEFAULT_CELL_PADDING;
    int bottomTwips = DEFAULT_CELL_PADDING;
};


// 3. Unitatea de tabel (echivalent TABLE)
struct RtfCell {
    //std::vector<RtfParagraph> content; // O celulă conține paragrafe
    int colspan = 1;
    int rowspan = 1;
    bool isMergeFirst = false; // ⭐ NOU: Început de comasare orizontală (\clmgf)
    bool isMergeNext = false;  // ⭐ NOU: Continuare comasare orizontală (\clmrg)

    Style style;

    CellBorders borders;
    CellPadding padding;

    std::vector<std::unique_ptr<RtfBlock>> content;

    RtfCell() = default;
    RtfCell(const RtfCell&) = delete;
    RtfCell& operator=(const RtfCell&) = delete;

    // Permite Mutarea (Ar trebui să fie implicit, dar o facem explicit)
    RtfCell(RtfCell&&) = default;
    RtfCell& operator=(RtfCell&&) = default;
};

struct RtfRow {
    std::vector<RtfCell> cells; // O celulă conține paragrafe
    std::vector<double> columnWidthsPt;
    bool isHeader = false;

    RtfRow() = default;
    RtfRow(const RtfRow&) = delete;
    RtfRow& operator=(const RtfRow&) = delete;
    RtfRow(RtfRow&&) = default;
    RtfRow& operator=(RtfRow&&) = default;
  
};

struct RtfTable : public RtfBlock {
    std::vector<RtfRow> rows; // O celulă conține paragrafe
    std::vector<double> columnWidthsPt;
   long lastCellBoundaryTwips = 0;

};

// Structura pentru a urmări stilul și starea curentă
struct RtfParseState {
    Style currentStyle;
    Style globalDefaultStyle;
    RtfPage& pageConfig;
    std::wstring currentTextBuffer;

    std::vector<bool> metadataStack;
    bool isParsingMetadata = false;
    // Pentru a gestiona stiva de stiluri (la intrarea în {})
    std::vector<Style> styleStack;
    std::wstring currentText;
    // Pointer către paragraful care se construiește
    std::unique_ptr<RtfParagraph> currentParagraph = nullptr;

    // Pentru a urmări dacă suntem într-un tabel
    bool inTable = false;
    RtfRow currentRow; // Buffer pentru celulele curente
    std::unique_ptr<RtfTable> currentTable = nullptr;

    BorderSpec currentBorderSpec;
    CellBorders currentCellBorders;
    CellPadding currentCellPadding;

    bool currentCellMergeFirst = false; // ⭐ NOU
    bool currentCellMergeNext = false;  // ⭐ NOU

    // ⭐ NOU: Flag-uri pentru pozițiile de bordură așteptate
    bool borderLeftPending = false;
    bool borderRightPending = false;
    bool borderTopPending = false;
    bool borderBottomPending = false;

    int currentCellIndex = -1;
    RtfCell* currentCell = nullptr;

    unsigned int ansicpg ;
    
    std::map<long, std::wstring> fontTable;
    bool parsingFontTable = false; // <-- Flag-ul nou
    long currentFontIndexForTable = -1;

    bool isParsingFooter = false;
    bool isParsingHeader = false;

    std::vector<bool> footerStack;
    std::vector<bool> headerStack;

    int ucCount = 1;             // Implicat în RTF: \uc1 (1 caracter fallback)
    int skipCharsRemaining = 0;  // Câte caractere fallback trebuie sărite după \uN

    RtfParseState() = default;
    RtfParseState(RtfPage& page)
        : pageConfig(page) {
        // Setați default-urile RTF:
        this->globalDefaultStyle.fontSize = 8.0;
        this->globalDefaultStyle.fontFamily = L"Arial Narrow";
        // ... etc.
        this->currentStyle = this->globalDefaultStyle;
        // Inițializare implicită

        this->ucCount = 1;
        this->skipCharsRemaining = 0;
    }
    RtfParseState(const RtfParseState&) = delete; // Nu vrem să copiem starea
    RtfParseState(RtfParseState&&) = default;
    RtfParseState& operator=(RtfParseState&&) = default;
};


void printRtfBlock(const RtfBlock& block, int depth = 0);
void printRtfParagraph(const RtfParagraph& paragraph, int depth);
void printRtfTable(const RtfTable& table, int depth);
void printRtfCell(const RtfCell& cell, int depth);


class Rtf {
private:
    std::vector<RtfPage> pageConfigurations;

    std::vector<std::unique_ptr<RtfBlock>> blocks; // RtfBlock poate fi Paragraph sau Table

    std::vector<std::unique_ptr<RtfBlock>> parseRtfContent(const std::string& content);

    std::vector<std::unique_ptr<RtfBlock>> headerBlocks;
    std::vector<std::unique_ptr<RtfBlock>> footerBlocks;

public:
    Rtf() = default;
    bool load(const std::wstring& filePath);

    bool loadFromString(const std::string& rtfContent) {
        if (rtfContent.empty()) {
            return false;
        }

        // Curățăm starea anterioară
        blocks.clear();
        pageConfigurations.clear();
        headerBlocks.clear();
        footerBlocks.clear();

        // Apelăm parser-ul intern
        blocks = parseRtfContent(rtfContent);

        return !blocks.empty();
    }

    // ⭐ NOU (Opțional): Suport pentru std::vector<uint8_t> / std::vector<char>
    bool loadFromMemory(const std::vector<uint8_t>& buffer) {
        if (buffer.empty()) return false;
        std::string content(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        return loadFromString(content);
    }


    void addBlock(std::unique_ptr<RtfBlock> block) {
        blocks.push_back(std::move(block));
    }



    // Funcție pentru a accesa blocurile (Exemplu)
    const std::vector<std::unique_ptr<RtfBlock>>& getBlocks() const {
        return blocks;
    }

    void print() const;

    void applyPendingBorders(RtfParseState& state);
    // ⭐ MODIFICAT: Funcție pentru a accesa configurația paginii curente/inițiale
        // Puteți returna prima (implicită) sau ultima (cea curentă/activă)
    const RtfPage& getPageInfo() const {
        if (!pageConfigurations.empty()) {
            return pageConfigurations.back(); // Returnează ultima configurație definită
        }
      
        throw std::runtime_error("No page configuration found in RTF document.");
    }

    const std::vector<std::unique_ptr<RtfBlock>>& getFooterBlocks() const { return footerBlocks; }
    const std::vector<std::unique_ptr<RtfBlock>>& getHeaderBlocks() const { return headerBlocks; }

    private:
private:
    std::unique_ptr<RtfBlock> finalizeCurrentParagraph(RtfParseState& state, std::vector<std::unique_ptr<RtfBlock>>& parsedBlocks);
    void handleControlWord(RtfParseState& state, const std::wstring& word, int param, std::vector<std::unique_ptr<RtfBlock>>& parsedBlocks);
    void finalizeCurrentSpan(RtfParseState& state);


        // ⭐ NOU: Funcție pentru a adăuga o nouă configurație de pagină
        void addPageConfiguration(const RtfPage& pageConfig) {
            pageConfigurations.push_back(pageConfig);
        }
}; 