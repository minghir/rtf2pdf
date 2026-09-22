
#include "RenderingContex.hpp"

void Table::print() {
    const int maxRows = static_cast<int>(rows.size());
    int maxCols = 0;

    // Estimăm numărul maxim de coloane
    for (const auto& row : rows) {
        int colCount = 0;
        for (const auto& cell : row.cells) {
            colCount += cell.colspan;
        }
        if (colCount > maxCols)
            maxCols = colCount;
    }

    // Inițializăm matricea cu "-"
    std::vector<std::vector<std::wstring>> matrix(maxRows, std::vector<std::wstring>(maxCols, L"-"));

    // Umplem matricea
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const auto& row = rows[rowIndex];
        int colIndex = 0;

        for (const auto& cell : row.cells) {
            // Găsim prima poziție liberă pe rând
            while (colIndex < maxCols && matrix[rowIndex][colIndex] != L"-")
                ++colIndex;

            // Determinăm simbolul: "X" dacă se extinde, altfel "1"
            std::wstring marker = (cell.rowspan > 1 || cell.colspan > 1) ? L"X" : L"1";

            // Aplicăm rowspan și colspan
            for (int r = 0; r < cell.rowspan; ++r) {
                for (int c = 0; c < cell.colspan; ++c) {
                    int targetRow = static_cast<int>(rowIndex + r);
                    int targetCol = static_cast<int>(colIndex + c);
                    if (targetRow < maxRows && targetCol < maxCols) {
                        matrix[targetRow][targetCol] = (r == 0 && c == 0) ? marker : L"X";
                    }
                }
            }

            colIndex += cell.colspan;
        }
    }

    // Tipărim matricea
    for (const auto& row : matrix) {
        std::wstring output;
        for (size_t i = 0; i < row.size(); ++i) {
            output += row[i];
            if (i < row.size() - 1)
                output += L"\t";
        }
        LOG_INFO(output);
    }
}





