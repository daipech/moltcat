#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace moltcat::utils {

/**
 * @brief Mathematical utility class
 *
 * Provides vector calculations, similarity computations, and other mathematical functions
 * Mainly serves the vector retrieval service of the Memory system
 */
class MathUtils {
public:
    // ========== Vector operations ==========

    /**
     * @brief Calculate vector dot product
     */
    template<typename T>
    [[nodiscard]] static auto dot_product(const std::vector<T>& a,
                                         const std::vector<T>& b) -> T {
        if (a.size() != b.size()) {
            throw std::invalid_argument("Vector dimension mismatch");
        }
        return std::inner_product(a.begin(), a.end(), b.begin(), T{0});
    }

    /**
     * @brief Calculate vector L2 norm (Euclidean norm)
     */
    template<typename T>
    [[nodiscard]] static auto l2_norm(const std::vector<T>& vec) -> double {
        double sum = 0.0;
        for (const auto& v : vec) {
            sum += static_cast<double>(v) * v;
        }
        return std::sqrt(sum);
    }

    /**
     * @brief Calculate vector L1 norm
     */
    template<typename T>
    [[nodiscard]] static auto l1_norm(const std::vector<T>& vec) -> double {
        double sum = 0.0;
        for (const auto& v : vec) {
            sum += std::abs(static_cast<double>(v));
        }
        return sum;
    }

    /**
     * @brief Vector normalization (L2 normalization)
     */
    template<typename T>
    [[nodiscard]] static auto normalize(const std::vector<T>& vec) -> std::vector<double> {
        auto norm = l2_norm(vec);
        if (norm < 1e-10) {
            throw std::runtime_error("Cannot normalize zero vector");
        }

        std::vector<double> result(vec.size());
        for (size_t i = 0; i < vec.size(); ++i) {
            result[i] = static_cast<double>(vec[i]) / norm;
        }
        return result;
    }

    /**
     * @brief Vector addition
     */
    template<typename T>
    [[nodiscard]] static auto add(const std::vector<T>& a,
                                  const std::vector<T>& b) -> std::vector<T> {
        if (a.size() != b.size()) {
            throw std::invalid_argument("Vector dimension mismatch");
        }

        std::vector<T> result(a.size());
        for (size_t i = 0; i < a.size(); ++i) {
            result[i] = a[i] + b[i];
        }
        return result;
    }

    /**
     * @brief Vector subtraction
     */
    template<typename T>
    [[nodiscard]] static auto sub(const std::vector<T>& a,
                                  const std::vector<T>& b) -> std::vector<T> {
        if (a.size() != b.size()) {
            throw std::invalid_argument("Vector dimension mismatch");
        }

        std::vector<T> result(a.size());
        for (size_t i = 0; i < a.size(); ++i) {
            result[i] = a[i] - b[i];
        }
        return result;
    }

    /**
     * @brief Vector scalar multiplication
     */
    template<typename T>
    [[nodiscard]] static auto scale(const std::vector<T>& vec,
                                    T scalar) -> std::vector<T> {
        std::vector<T> result(vec.size());
        for (size_t i = 0; i < vec.size(); ++i) {
            result[i] = vec[i] * scalar;
        }
        return result;
    }

    // ========== Similarity computation ==========

    /**
     * @brief Calculate cosine similarity
     *
     * @param a Vector A
     * @param b Vector B
     * @return Similarity [-1, 1], 1 means identical, -1 means completely opposite
     */
    template<typename T>
    [[nodiscard]] static auto cosine_similarity(const std::vector<T>& a,
                                                const std::vector<T>& b) -> double {
        if (a.size() != b.size()) {
            throw std::invalid_argument("Vector dimension mismatch");
        }

        double dot = 0.0;
        double norm_a = 0.0;
        double norm_b = 0.0;

        for (size_t i = 0; i < a.size(); ++i) {
            double va = static_cast<double>(a[i]);
            double vb = static_cast<double>(b[i]);
            dot += va * vb;
            norm_a += va * va;
            norm_b += vb * vb;
        }

        double denominator = std::sqrt(norm_a) * std::sqrt(norm_b);
        if (denominator < 1e-10) {
            return 0.0;  // Zero vector, similarity is 0
        }

        return dot / denominator;
    }

    /**
     * @brief Calculate cosine distance
     *
     * @param a Vector A
     * @param b Vector B
     * @return Distance [0, 2], 0 means identical
     */
    template<typename T>
    [[nodiscard]] static auto cosine_distance(const std::vector<T>& a,
                                              const std::vector<T>& b) -> double {
        return 1.0 - cosine_similarity(a, b);
    }

    /**
     * @brief Calculate Euclidean distance
     */
    template<typename T>
    [[nodiscard]] static auto euclidean_distance(const std::vector<T>& a,
                                                  const std::vector<T>& b) -> double {
        if (a.size() != b.size()) {
            throw std::invalid_argument("Vector dimension mismatch");
        }

        double sum = 0.0;
        for (size_t i = 0; i < a.size(); ++i) {
            double diff = static_cast<double>(a[i]) - static_cast<double>(b[i]);
            sum += diff * diff;
        }
        return std::sqrt(sum);
    }

    /**
     * @brief Calculate Manhattan distance
     */
    template<typename T>
    [[nodiscard]] static auto manhattan_distance(const std::vector<T>& a,
                                                 const std::vector<T>& b) -> double {
        if (a.size() != b.size()) {
            throw std::invalid_argument("Vector dimension mismatch");
        }

        double sum = 0.0;
        for (size_t i = 0; i < a.size(); ++i) {
            sum += std::abs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
        }
        return sum;
    }

    /**
     * @brief Calculate Jaccard similarity
     *
     * Applicable to binary vectors or sets
     */
    template<typename T>
    [[nodiscard]] static auto jaccard_similarity(const std::vector<T>& a,
                                                 const std::vector<T>& b) -> double {
        if (a.empty() && b.empty()) {
            return 1.0;
        }

        double intersection = 0.0;
        double union_size = 0.0;

        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) {
            if (a[i] < b[j]) {
                ++i;
                union_size += 1.0;
            } else if (a[i] > b[j]) {
                ++j;
                union_size += 1.0;
            } else {
                intersection += 1.0;
                union_size += 1.0;
                ++i;
                ++j;
            }
        }

        union_size += static_cast<double>(a.size() - i);
        union_size += static_cast<double>(b.size() - j);

        if (union_size < 1e-10) {
            return 0.0;
        }

        return intersection / union_size;
    }

    // ========== Statistical functions ==========

    /**
     * @brief Calculate vector mean
     */
    template<typename T>
    [[nodiscard]] static auto mean(const std::vector<T>& vec) -> double {
        if (vec.empty()) {
            return 0.0;
        }

        double sum = std::accumulate(vec.begin(), vec.end(), 0.0);
        return sum / static_cast<double>(vec.size());
    }

    /**
     * @brief Calculate vector standard deviation
     */
    template<typename T>
    [[nodiscard]] static auto stddev(const std::vector<T>& vec) -> double {
        if (vec.size() < 2) {
            return 0.0;
        }

        double avg = mean(vec);
        double sum_sq = 0.0;

        for (const auto& v : vec) {
            double diff = static_cast<double>(v) - avg;
            sum_sq += diff * diff;
        }

        return std::sqrt(sum_sq / static_cast<double>(vec.size() - 1));
    }

    /**
     * @brief Calculate vector variance
     */
    template<typename T>
    [[nodiscard]] static auto variance(const std::vector<T>& vec) -> double {
        double std = stddev(vec);
        return std * std;
    }

    /**
     * @brief Find maximum value in vector and its index
     */
    template<typename T>
    [[nodiscard]] static auto max_with_index(const std::vector<T>& vec) -> std::pair<T, size_t> {
        if (vec.empty()) {
            throw std::invalid_argument("Vector cannot be empty");
        }

        auto max_it = std::max_element(vec.begin(), vec.end());
        size_t index = std::distance(vec.begin(), max_it);
        return {*max_it, index};
    }

    /**
     * @brief Find minimum value in vector and its index
     */
    template<typename T>
    [[nodiscard]] static auto min_with_index(const std::vector<T>& vec) -> std::pair<T, size_t> {
        if (vec.empty()) {
            throw std::invalid_argument("Vector cannot be empty");
        }

        auto min_it = std::min_element(vec.begin(), vec.end());
        size_t index = std::distance(vec.begin(), min_it);
        return {*min_it, index};
    }

    // ========== Probability and distributions ==========

    /**
     * @brief Softmax function
     *
     * Convert vector to probability distribution
     */
    [[nodiscard]] static auto softmax(const std::vector<double>& vec) -> std::vector<double> {
        if (vec.empty()) {
            return {};
        }

        // Find maximum value (numerical stability)
        double max_val = *std::max_element(vec.begin(), vec.end());

        // Calculate exp and sum
        std::vector<double> result(vec.size());
        double sum = 0.0;

        for (size_t i = 0; i < vec.size(); ++i) {
            result[i] = std::exp(vec[i] - max_val);
            sum += result[i];
        }

        // Normalize
        for (auto& v : result) {
            v /= sum;
        }

        return result;
    }

    /**
     * @brief Sigmoid function
     */
    [[nodiscard]] static auto sigmoid(double x) -> double {
        return 1.0 / (1.0 + std::exp(-x));
    }

    /**
     * @brief Tanh function
     */
    [[nodiscard]] static auto tanh(double x) -> double {
        return std::tanh(x);
    }

    /**
     * @brief ReLU function
     */
    [[nodiscard]] static auto relu(double x) -> double {
        return std::max(0.0, x);
    }

    // ========== Interpolation functions ==========

    /**
     * @brief Linear interpolation
     */
    [[nodiscard]] static auto lerp(double a, double b, double t) -> double {
        return a + t * (b - a);
    }

    /**
     * @brief Smooth interpolation
     */
    [[nodiscard]] static auto smoothstep(double edge0, double edge1, double x) -> double {
        double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }

    // ========== Common mathematical constants ==========

    static constexpr double PI = 3.14159265358979323846;
    static constexpr double E = 2.71828182845904523536;
    static constexpr double SQRT2 = 1.41421356237309504880;
    static constexpr double LN2 = 0.69314718055994530942;
    static constexpr double GOLDEN_RATIO = 1.6180339887498948482;

    // ========== Utility functions ==========

    /**
     * @brief Convert degrees to radians
     */
    [[nodiscard]] static auto degrees_to_radians(double degrees) -> double {
        return degrees * PI / 180.0;
    }

    /**
     * @brief Convert radians to degrees
     */
    [[nodiscard]] static auto radians_to_degrees(double radians) -> double {
        return radians * 180.0 / PI;
    }

    /**
     * @brief Clamp value to specified range
     */
    template<typename T>
    [[nodiscard]] static auto clamp(T value, T min_val, T max_val) -> T {
        return std::clamp(value, min_val, max_val);
    }

    /**
     * @brief Check if approximately equal (floating point)
     */
    [[nodiscard]] static auto is_approx_equal(double a,
                                               double b,
                                               double epsilon = 1e-6) -> bool {
        return std::abs(a - b) < epsilon;
    }

    /**
     * @brief Check if value is within range
     */
    template<typename T>
    [[nodiscard]] static auto is_in_range(T value, T min_val, T max_val) -> bool {
        return value >= min_val && value <= max_val;
    }
};

} // namespace moltcat::utils
