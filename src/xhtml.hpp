#ifndef XHTML_HPP
#define XHTML_HPP
#include "css.hpp"

#include <string>
#include <vector>
#include <map>
#include <pugixml.hpp> // Vom folosi pugixml::xml_node intern

class XhtmlElement {
public:
    // Identificatorii elementului
    std::wstring tagName; // ex: "body", "p", "h1"
    std::wstring content; // Conținutul text (pentru noduri ca <p>Text</p>)
    // ID unic intern
    std::size_t internalId = 0;

    // Atributele elementului: <div id="A" class="B">
    std::map<std::wstring, std::wstring> attributes;
    
    
    // Relația ierarhică: Sub-elementele
    std::vector<XhtmlElement> subElements;

public:
    // Constructorul
    XhtmlElement() = default;

    // Metodă pentru a adăuga un subelement (manual)
    void addSubElement(const XhtmlElement& element);

    // Metodă de interogare
    bool hasAttribute(const std::wstring& name) const;
    std::wstring getAttribute(const std::wstring& name) const;

    // Metodă statică sau internă pentru a popula structura din pugixml::xml_node
    static XhtmlElement fromPugiNode(const pugi::xml_node& node);

    // TODO: adăugați metode pentru manipularea (ex: appendChild, removeChild)
    std::wstring getTagName() const { return tagName; }
    std::wstring getTagContent() const { return content; }

    bool multiPage() const {
        return tagName == L"html" || tagName == L"body" || tagName == L"div" || tagName == L"p"
            || tagName == L"table";
    }

    std::size_t getTagId() const { return internalId; }
};

// xhtml.hpp (Partea a 2-a)

class Xhtml {
private:
    pugi::xml_document m_pugiDocument; // Obiectul de parsare Pugixml
    XhtmlElement m_rootElement;           // Rădăcina arborelui nostru abstractizat
    CssDefinition m_styles;

    std::size_t m_nextId = 1;
public:
    Xhtml() = default;

    // 1. Încărcare & Parsare
    bool load(const std::wstring& filePath);

    // 2. Acces la arborele abstractizat
    const XhtmlElement& getRoot() const {
        return m_rootElement;
    }

    // 3. (Opțional) Căutare rapidă (folosind încă Pugixml intern, sau recursivitate)
    // De exemplu, căutarea nodului <body>
    XhtmlElement* findElementByTagName(const std::wstring& tagName);

    // 4. Salvarea
    bool save(const std::wstring& filePath) const;
    void printHtml() const;
    CssDefinition getStyles() const { return m_styles; }

    bool hasAttribute(const std::wstring& name) const;
    std::wstring getAttribute(const std::wstring& name) const;
    

    std::map<std::wstring, std::wstring> getStylesForElement(const XhtmlElement& element) const;

    XhtmlElement* getElementById(const std::wstring& id);

    const XhtmlElement* getElementByInternalId(std::size_t id) const;

private:
    // Metodă recursivă privată pentru a converti arborele pugixml în arbore XhtmlElement
    void parseNodeRecursive(const pugi::xml_node& pugiNode, XhtmlElement& parentElement);

    // Metodă recursivă privată pentru a inversa procesul (salvare)
    void buildPugiNodeRecursive(pugi::xml_node& pugiNode, const XhtmlElement& sourceElement) const;
    void printNodeRecursive(const XhtmlElement& element, int depth) const;
    XhtmlElement* getElementByIdRecursive(XhtmlElement& element, const std::wstring& id);
    const XhtmlElement* getElementByInternalIdRecursive(const XhtmlElement& element, std::size_t id) const;
};
#endif