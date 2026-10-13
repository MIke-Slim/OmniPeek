#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

template <typename T>
class MatrixBuffer {
private:
    std::vector<T> m_data;
    size_t m_rows, m_cols;
public:
    MatrixBuffer(size_t rows, size_t cols) : m_rows(rows), m_cols(cols), m_data(rows * cols) {}
    T& at(size_t r, size_t c) { return m_data[r * m_cols + c]; }
    size_t size() const { return m_data.size(); }
};

int main() {
    std::cout << "OmniPeek High-Performance Native Engine Initialized.\n";
    MatrixBuffer<float> mat(4, 4);
    mat.at(0, 0) = 1.0f;
    return 0;
}
