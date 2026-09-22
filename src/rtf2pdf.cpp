#include "ConsoleManager.hpp"
#include "rtf.hpp"
#include "RTFtoPDFConverter.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <codecvt>
#include <locale>

// Funcțiile tale rămân intacte
bool tdocsRTFtoPDFMemory(const std::string& rtfContentInMemory, std::vector<uint8_t>& outPdfBuffer) {
    Rtf rtfDoc;

    if (!rtfDoc.loadFromString(rtfContentInMemory)) {
        LOG_ERROR(L"[RTF2PDF] Pasul 1 ESUAT: Rtf::loadFromString nu a putut parsa string-ul RTF!");
        return false;
    }
    LOG_SUCCESS(L"[RTF2PDF] Pasul 1 REUSIT: RTF parsat corect.");

    RtfToPdfConverter converter(rtfDoc);
    if (!converter.convertToMemory(outPdfBuffer)) {
        LOG_ERROR(L"[RTF2PDF] Pasul 2 ESUAT: RtfToPdfConverter::convertToMemory a returnat false!");
        return false;
    }

    LOG_SUCCESS(L"[RTF2PDF] Pasul 2 REUSIT: PDF generat! Dimensiune: " + std::to_wstring(outPdfBuffer.size()) + L" bytes.");
    return true;
}

bool tdocsRTFtoPDF(const std::wstring& rtfFile, const std::wstring& pdfDir) {
    Rtf rtfDoc;
    if (rtfDoc.load(rtfFile + L".rtf")) {
        RtfToPdfConverter converter(rtfDoc);
        if (converter.convert(pdfDir + rtfFile + L".pdf")) {
            return true;
        }
    }
    return false;
}

// Citire fișier binar în string
bool readTextFile(const std::string& filepath, std::string& outContent) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;

    outContent.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return true;
}

// Scriere PDF pe disc
bool writePdfFile(const std::string& filepath, const std::vector<uint8_t>& buffer) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return true;
}

// Helper local pentru conversie wchar_t* la UTF-8 std::string
std::string wstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
#ifdef _WIN32
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
#endif
}

bool tdocsRTFtoPDF2(const std::wstring& rtfPath, const std::wstring& pdfPath) {
    Rtf rtfDoc;
    if (rtfDoc.load(rtfPath)) {
        RtfToPdfConverter converter(rtfDoc);
        if (converter.convert(pdfPath)) {
            return true;
        }
    }
    return false;
}


#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
    // Inițializăm ConsoleManager (configurează corect UTF-8 și stream-urile)
    ConsoleManager::getInstance().initialize();

    LOG_INFO(L"[rtf2pdf] Pornire utilitar de conversie RTF -> PDF...");

    if (argc < 3) {
        LOG_WARNING(L"Utilizare corectă: rtf2pdf.exe <fisier_intrare.rtf> <fisier_iesire.pdf>");
        ConsoleManager::getInstance().shutdown();
        return 1;
    }

    // Preluăm argumentele Unicode primite prin wmain și le convertim în string pentru citire fișiere
    std::string inputPath = wstringToUtf8(argv[1]);
    std::string outputPath = wstringToUtf8(argv[2]);

    LOG_INFO(L"Fișier intrare: " + std::wstring(argv[1]));
    LOG_INFO(L"Fișier ieșire:  " + std::wstring(argv[2]));
    /*
    // 1. Citim fișierul RTF
    std::string rtfContent;
    if (!readTextFile(inputPath, rtfContent)) {
        LOG_ERROR(L"[rtf2pdf] Eșec la citirea fișierului RTF de pe disc!");
        ConsoleManager::getInstance().shutdown();
        return 1;
    }

    // 2. Convertim în memorie cu funcția ta
    std::vector<uint8_t> pdfBuffer;
    if (!tdocsRTFtoPDFMemory(rtfContent, pdfBuffer)) {
        LOG_ERROR(L"[rtf2pdf] Conversia RTF la PDF în memorie a eșuat.");
        ConsoleManager::getInstance().shutdown();
        return 1;
    }

    // 3. Salvăm PDF-ul rezultat
    if (!writePdfFile(outputPath, pdfBuffer)) {
        LOG_ERROR(L"[rtf2pdf] Nu s-a putut salva fișierul PDF de ieșire pe disc!");
        ConsoleManager::getInstance().shutdown();
        return 1;
    }
    */

    std::wstring inputFile = argv[1];   // "./test/sample3.rtf"
    std::wstring outputDir = argv[2];   // "./"

    size_t pos = inputFile.find_last_of(L"/\\");
    std::wstring dir = (pos == std::wstring::npos) ? L"" : inputFile.substr(0, pos + 1);

    std::wstring file = inputFile.substr(pos + 1); // "sample3.rtf"
    file = file.substr(0, file.find_last_of(L".")); // "sample3"

    if (tdocsRTFtoPDF2(argv[1], argv[2])) {
		LOG_SUCCESS(L"[rtf2pdf] Conversie finalizată cu succes!");
	}
	else {
		LOG_ERROR(L"[rtf2pdf] Conversia RTF la PDF a eșuat.");
		ConsoleManager::getInstance().shutdown();
		return 1;
	}   

    LOG_SUCCESS(L"[rtf2pdf] Conversie finalizată cu succes!");

    ConsoleManager::getInstance().shutdown();
    return 0;
}
#else
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::wcerr << L"Utilizare: rtf2pdf <fisier_intrare.rtf> <fisier_iesire.pdf>\n";
        return 1;
    }

    ConsoleManager::getInstance().initialize();
    LOG_INFO(L"[rtf2pdf] Pornire utilitar de conversie RTF -> PDF...");

    std::string rtfContent;
    if (!readTextFile(argv[1], rtfContent)) {
        LOG_ERROR(L"[rtf2pdf] Eșec la citirea fișierului RTF de pe disc!");
        return 1;
    }

    std::vector<uint8_t> pdfBuffer;
    if (!tdocsRTFtoPDFMemory(rtfContent, pdfBuffer) || !writePdfFile(argv[2], pdfBuffer)) {
        LOG_ERROR(L"[rtf2pdf] Conversia sau salvarea PDF a eșuat.");
        return 1;
    }

    LOG_SUCCESS(L"[rtf2pdf] Conversie finalizată cu succes!");
    return 0;
}
#endif