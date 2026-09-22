#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <iostream>


struct CssProperty {
    std::wstring name;  // ex: "font-size", "margin"
    std::wstring value; // ex: "12pt", "10mm auto"
};


// Definește structurile de bază (poți le poți muta aici)
struct CssRule {
    std::wstring selector;
    std::map<std::wstring, std::wstring> properties;

    CssRule(const std::wstring& sel) : selector(sel) {}

    // Ajută la debug
    std::wstring toString() const {
        std::wstringstream ss;
        ss << L"Regula [" << selector << L"]:\n";
        for (const auto& prop : properties) {
            ss << L"  - " << prop.first << L": " << prop.second << L";\n";
        }
        return ss.str();
    }
};

class CssDefinition {
private:
    std::vector<CssRule> m_rules;

    // Funcție utilitară pentru a elimina spațiile albe de la început și sfârșit
    std::wstring trim(const std::wstring& str);

public:
    CssDefinition() = default;

    // Adaugă o regulă nouă (folosit intern de parser)
    void addRule(const CssRule& rule);

    // Adaugă/Setează o proprietate la o regulă existentă
    void setProperty(const std::wstring& selector,
        const std::wstring& propertyName,
        const std::wstring& propertyValue);

    // Metodă cheie: Procesează conținutul text brut (din blocul CDATA)
    bool parseCssContent(const std::wstring& cssText);

    // Obține valoarea unei proprietăți
    std::wstring getPropertyValue(const std::wstring& selector,
        const std::wstring& propertyName) const;

    // Metodă pentru a vedea toate regulile (Debug)
    std::wstring getAllRulesAsString() const;

    void printRules() const;

    std::map<std::wstring, std::wstring> getRule(const std::wstring& selector) const;



protected:
    
    std::vector<std::wstring> splitSelectors(const std::wstring& compositeSelector);
};