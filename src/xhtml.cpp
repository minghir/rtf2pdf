// În xhtml.cpp
#include "xhtml.hpp"
#include "ConsoleManager.hpp"
#include "stringUtils.hpp"

#include <algorithm> // Aici se află std::all_of
#include <cctype>    // Aici se află funcțiile de tip iswspace (pentru wchar_t)
#include <cwctype>



void Xhtml::parseNodeRecursive(const pugi::xml_node& pugiNode, XhtmlElement& parentElement) {

    for (pugi::xml_node child = pugiNode.first_child(); child; child = child.next_sibling()) {

        std::string childTagNameStr = child.name();
        std::wstring childTagName = str_to_wstr(childTagNameStr);

        // Ignorăm nodurile care nu ne interesează direct (comentarii, DTD)
        if (child.type() == pugi::node_comment || child.type() == pugi::node_declaration) {
            continue;
        }

        // Cazul 1: Nod text sau CDATA (CONSOLIDAT)
        if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {

            // Folosim str_to_wstr, presupunând că child.value() este UTF-8 și că aveți nevoie de wstring.
            //std::wstring rawContent = str_to_wstr(child.value());
            std::wstring rawContent = utf8_to_wstring(child.value());


            // --- 1. DETECTAREA SPAȚIILOR ---
            // Verificăm dacă nodul text are spațiu la început sau la sfârșit ÎNAINTE de a-l trim-ui.
            bool has_leading_space = !rawContent.empty() && std::iswspace(rawContent.front());
            bool has_trailing_space = !rawContent.empty() && std::iswspace(rawContent.back());

            // --- 2. NORMALIZAREA CONȚINUTULUI ---
            // Eliminăm spațiile de început și de sfârșit pentru a obține conținutul curat
            std::wstring normalizedContent = wstr_trim(rawContent); // Folosim wstr_trim

            if (normalizedContent.empty()) {
                // Dacă nodul text conținea doar spații albe, îl ignorăm complet.
                continue;
            }

            // --- 3. CREAREA ELEMENTULUI #TEXT ---
            XhtmlElement textElement;
            textElement.tagName = L"#text"; // Marcaj pentru nod text
            textElement.content = normalizedContent;
            textElement.internalId = m_nextId++;

            // --- 4. SETAREA ATRIBUTELOR SPECIALE ---
            if (has_leading_space) {
                textElement.attributes[L"__xhtml_leading_space"] = L"true";
                LOG_DEBUG(L"[Xhtml::parseNodeRecursive] Nod text normalizat: a avut spațiu de început.");
            }
            if (has_trailing_space) {
                textElement.attributes[L"__xhtml_trailing_space"] = L"true";
                LOG_DEBUG(L"[Xhtml::parseNodeRecursive] Nod text normalizat: a avut spațiu de sfârșit.");
            }

            // --- 5. ADAUGARE LA PĂRINTE ---
            parentElement.subElements.push_back(std::move(textElement));
            LOG_INFO(L"[Xhtml::parseNodeRecursive] Adaugă nod text normalizat la părintele <" + parentElement.tagName + L"> (Conținut: \"" + normalizedContent + L"\")");

            // Trecem la următorul 'child' din buclă
            continue;
        }

        // Cazul 2: Nod de tip element (ex: <div>, <p>)
        if (child.type() == pugi::node_element) {
            // ... (logica de procesare a elementelor structurale rămâne neschimbată)
            XhtmlElement newElement;
            newElement.tagName = childTagName;
            newElement.internalId = m_nextId++;


            // Logare atribute
            size_t attrCount = 0;
            for (const auto& attr : child.attributes()) {
                std::string attrNameStr = attr.name();
                std::string attrValueStr = attr.value();
                newElement.attributes[str_to_wstr(attrNameStr)] = str_to_wstr(attrValueStr);
                attrCount++;
            }
            LOG_INFO(L"[Xhtml::parseNodeRecursive] Procesează element <" + childTagName + L"> cu " + std::to_wstring(attrCount) + L" atribute.");

            // Recurență: Parcurgem copiii noului element
            parseNodeRecursive(child, newElement);

            if (newElement.tagName == L"style") {
                // NOU: Verificăm dacă există un sub-element, care ar trebui să fie nodul #text/CDATA cu conținutul CSS.
                if (!newElement.subElements.empty()) {
                    const XhtmlElement& cssContentNode = newElement.subElements[0];

                    // Asigurăm că nodul este de tip text și că nu este gol
                    if (cssContentNode.tagName == L"#text" && !cssContentNode.content.empty()) {

                        std::wstring cssContent = cssContentNode.content;

                        LOG_INFO(L"[Xhtml::parseNodeRecursive] S-a detectat conținut CSS în <style>. Începe parsarea CSS.");

                        // Aici apelezi parserul CSS
                        m_styles.parseCssContent(cssContent);

                        // Verifică imediat dacă regulile au fost stocate
                        m_styles.printRules();
                    }
                }
            }

            // Adăugăm noul element la părinte
            parentElement.subElements.push_back(std::move(newElement));
            LOG_INFO(L"[Xhtml::parseNodeRecursive] Element <" + childTagName + L"> adăugat la părintele <" + parentElement.tagName + L">.");
        }
    }
}


bool Xhtml::load(const std::wstring& filePath) {
    LOG_INFO(L"[Xhtml::load] Începe încărcarea și parsarea fișierului: " + filePath);

    // Convertim calea în std::string pentru pugixml
    std::string path(filePath.begin(), filePath.end());

    // 1. Parsarea documentului
    pugi::xml_parse_result result = m_pugiDocument.load_file(path.c_str());

    if (!result) {
        std::wstring errorMsg = L"[Xhtml::load] Eroare parsare XML. Descriere: ";
        // Aici ar trebui să folosești o funcție de conversie string -> wstring pentru result.description()
        // errorMsg += str_to_wstr(result.description()); 
        LOG_ERROR(errorMsg + L" (Detalii: Vezi eroare Pugixml)");
        return false;
    }
    LOG_SUCCESS(L"[Xhtml::load] Parsare Pugixml reușită.");


    // Obținem nodul rădăcină (ex: <html>)
    pugi::xml_node rootNode = m_pugiDocument.child("html");

    if (rootNode) {
        // Inițializăm rădăcina arborelui nostru
        m_rootElement.tagName = L"html";
        LOG_INFO(L"[Xhtml::load] Nod rădăcină 'html' identificat. Începe construcția arborelui abstractizat.");

        // Începem procesul recursiv de copiere/abstractizare
        parseNodeRecursive(rootNode, m_rootElement);

        LOG_SUCCESS(L"[Xhtml::load] Arborele XhtmlElement construit cu succes.");
        return true;
    }

    // Dacă nu găsim nodul <html>
    LOG_ERROR(L"[Xhtml::load] Documentul nu conține nodul rădăcină <html/>. Încărcare eșuată.");
    return false;
}

// Definire locală a constantelor de culoare pentru consistență (sau includeți header-ul unde sunt definite)
#ifndef HTML_COLOR_TAG
#define HTML_COLOR_TAG      (FOREGROUND_INTENSITY | FOREGROUND_RED) // Roșu pentru nume tag
#endif
#ifndef HTML_COLOR_ATTR_NAME
#define HTML_COLOR_ATTR_NAME (FOREGROUND_GREEN) // Verde pentru nume atribut
#endif
#ifndef HTML_COLOR_ATTR_VALUE
#define HTML_COLOR_ATTR_VALUE (FOREGROUND_INTENSITY | FOREGROUND_BLUE) // Albastru pentru valoare atribut
#endif
#ifndef HTML_COLOR_CONTENT
#define HTML_COLOR_CONTENT  (FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE) // Alb pentru conținut
#endif
#ifndef HTML_COLOR_BRACKET
#define HTML_COLOR_BRACKET  (FOREGROUND_INTENSITY) // Alb intens pentru paranteze unghiulare
#endif


// Metoda publică de pornire
void Xhtml::printHtml() const {
    ConsoleManager& console = ConsoleManager::getInstance();

    console.setColor(HTML_COLOR_CONTENT);
    std::wcout << L"\n--- XHMTL Document Structure ---\n";

    // Începe afișarea recursivă de la elementul rădăcină (root)
    printNodeRecursive(m_rootElement, 0);

    console.resetColor();
    std::wcout << L"\n-----------------------------------\n" << std::endl;
}


// Metoda recursivă
void Xhtml::printNodeRecursive(const XhtmlElement& element, int depth) const {
    ConsoleManager& console = ConsoleManager::getInstance();

    // Setează indentarea pe baza nivelului (depth)
    std::wstring indent(depth * 4, L' ');

    // ------------------------------------
    // 1. Tag de Deschidere
    // ------------------------------------
    console.setColor(HTML_COLOR_BRACKET);
    std::wcout << indent << L"<";

    console.setColor(HTML_COLOR_TAG);
    std::wcout << element.tagName;

    // Afișează atributele
    for (const auto& attr : element.attributes) {
        std::wcout << L" ";

        // Nume atribut
        console.setColor(HTML_COLOR_ATTR_NAME);
        std::wcout << attr.first;

        // Separator și valoare
        console.setColor(HTML_COLOR_BRACKET);
        std::wcout << L"=\"";

        console.setColor(HTML_COLOR_ATTR_VALUE);
        std::wcout << attr.second;

        console.setColor(HTML_COLOR_BRACKET);
        std::wcout << L"\"";
    }

    // Închide tag-ul de deschidere
    console.setColor(HTML_COLOR_BRACKET);

    // Verifică dacă este un tag gol (ex: <br/>)
    bool isEmptyTag = element.subElements.empty() && element.content.empty();

    if (isEmptyTag) {
        std::wcout << L" />\n";
    }
    else {
        std::wcout << L">\n";
    }

    // ------------------------------------
    // 2. Conținutul Text
    // ------------------------------------
    if (!element.content.empty()) {
        std::wstring trimmedContent = element.content; // Poți aplica trim aici

        // Dacă conținutul este prea lung (ex: bloc CDATA), afișăm doar o parte.
        if (trimmedContent.length() > 60) {
            trimmedContent = trimmedContent.substr(0, 60) + L"...";
        }

        // Setează culoarea pentru conținut
        console.setColor(HTML_COLOR_CONTENT);
        std::wcout << indent << L"    " << trimmedContent << L"\n";
    }

    // ------------------------------------
    // 3. Apel Recursiv pentru Sub-elemente
    // ------------------------------------
    for (const auto& subElement : element.subElements) {
        printNodeRecursive(subElement, depth + 1);
    }

    // ------------------------------------
    // 4. Tag de Închidere (dacă nu e tag gol)
    // ------------------------------------
    if (!isEmptyTag) {
        console.setColor(HTML_COLOR_BRACKET);
        std::wcout << indent << L"</";

        console.setColor(HTML_COLOR_TAG);
        std::wcout << element.tagName;

        console.setColor(HTML_COLOR_BRACKET);
        std::wcout << L">\n";
    }

    // Resetează culoarea
    console.resetColor();
}

bool XhtmlElement::hasAttribute(const std::wstring& name) const {
    return attributes.count(name) > 0;
}

std::wstring XhtmlElement::getAttribute(const std::wstring& name) const {
    auto it = attributes.find(name);
    if (it != attributes.end()) {
        return it->second;
    }
    // Returnează un string gol dacă atributul nu este găsit
    return L"";
}


std::map<std::wstring, std::wstring> Xhtml::getStylesForElement(const XhtmlElement& element) const {
    std::map<std::wstring, std::wstring> finalStyles;

    // 1. Stiluri din CSS definit (selectorii externi)
    std::vector<std::wstring> selectors;
    if (&element == nullptr) {
        LOG_ERROR( L"Xhtml::getStylesForElement: Element invalid!" );
        return {};
    }



    selectors.push_back(element.tagName); // ex: "p"

    if (element.hasAttribute(L"class")) {
        std::wstringstream ss(element.getAttribute(L"class"));
        std::wstring cls;
        while (std::getline(ss, cls, L' ')) {
            if (!cls.empty()) {
                // Adăugăm selectorul de clasă simplu: ".col1"
                selectors.push_back(L"." + cls);

                // Adăugăm selectorul compus: "td.col1" (CRUCIAL!)
                selectors.push_back(element.tagName + L"." + cls);
            }
            
        }
    }

    if (element.hasAttribute(L"id")) {
        selectors.push_back(L"#" + element.getAttribute(L"id"));
    }

    for (const auto& sel : selectors) {
        auto rule = m_styles.getRule(sel);
        for (const auto& [name, value] : rule) {
            finalStyles[name] = value; // suprascrie dacă există
        }
    }

    // 2. Stiluri inline (au prioritate maximă)
    if (element.hasAttribute(L"style")) {
        std::wstring inlineStyle = element.getAttribute(L"style");
        size_t pos = 0;
        while (pos < inlineStyle.length()) {
            size_t colon = inlineStyle.find(L':', pos);
            if (colon == std::wstring::npos) break;

            size_t semicolon = inlineStyle.find(L';', colon);
            if (semicolon == std::wstring::npos) semicolon = inlineStyle.length();

            std::wstring name_raw = inlineStyle.substr(pos, colon - pos);
            std::wstring value_raw = inlineStyle.substr(colon + 1, semicolon - colon - 1);

            std::wstring name = wstr_trim(name_raw);
            std::wstring value = wstr_trim(value_raw);

            finalStyles[name] = value; // suprascrie orice altă sursă
            pos = semicolon + 1;
        }
    }

    return finalStyles;
}

XhtmlElement* Xhtml::getElementById(const std::wstring& id) {
    return getElementByIdRecursive(m_rootElement, id);
}

XhtmlElement* Xhtml::getElementByIdRecursive(XhtmlElement& element, const std::wstring& id) {
        // Verificăm dacă elementul curent are atributul "id" și se potrivește
        auto it = element.attributes.find(L"id");
        if (it != element.attributes.end() && it->second == id) {
            return &element;
        }

        // Căutăm recursiv în subelemente
        for (auto& child : element.subElements) {
            XhtmlElement* found = getElementByIdRecursive(child, id);
            if (found) {
                return found;
            }
        }

        return nullptr; // nu am găsit
    }

const XhtmlElement* Xhtml::getElementByInternalIdRecursive(const XhtmlElement& element, size_t id) const {
    if (element.internalId == id) {
        return &element;
    }
    for (const auto& child : element.subElements) {
        const XhtmlElement* found = getElementByInternalIdRecursive(child, id);
        if (found) {
            return found;
        }
    }


    return nullptr;
}

const XhtmlElement* Xhtml::getElementByInternalId(size_t id) const {
    return getElementByInternalIdRecursive(m_rootElement, id);
}



/*
XhtmlElement* Xhtml::getElementByInternalIdRecursive(XhtmlElement& element, std::size_t id) {
        // Verificăm dacă elementul curent are ID-ul intern căutat
        if (element.internalId == id) {
            return &element;
        }

        // Căutăm recursiv în subelemente
        for (auto& child : element.subElements) {
            XhtmlElement* found = getElementByInternalIdRecursive(child, id);
            if (found) {
                return found;
            }
        }

        return nullptr; // nu am găsit
 }

 */