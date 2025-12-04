#ifndef ALGORITHM_PARAMETERS_H
#define ALGORITHM_PARAMETERS_H

#include <string>
#include <unordered_map>
#include <variant>
#include <stdexcept>

namespace core
{

    using ParamValue = std::variant<int, double, bool>;

    class AlgorithmParameters
    {
    public:
        void set(const std::string &name, ParamValue value)
        {
            m_values[name] = value;
        }

        void setFromString(const std::string &name, const std::string &value_str)
        {
            if (value_str == "true")
            {
                set(name, true);
                return;
            }
            if (value_str == "false")
            {
                set(name, false);
                return;
            }

            // Try int first (no decimal point)
            if (value_str.find('.') == std::string::npos)
            {
                try
                {
                    set(name, std::stoi(value_str));
                    return;
                }
                catch (...)
                {
                }
            }

            // Try double
            try
            {
                set(name, std::stod(value_str));
                return;
            }
            catch (...)
            {
                throw std::invalid_argument("Cannot parse: " + value_str);
            }
        }

        bool has(const std::string &name) const
        {
            return m_values.find(name) != m_values.end();
        }

        template <typename T>
        T get(const std::string &name) const
        {
            auto it = m_values.find(name);
            if (it == m_values.end())
            {
                throw std::out_of_range("Parameter not found: " + name);
            }

            if (auto *val = std::get_if<T>(&it->second))
            {
                return *val;
            }

            // Numeric conversion
            if constexpr (std::is_same_v<T, double>)
            {
                if (auto *val = std::get_if<int>(&it->second))
                {
                    return static_cast<double>(*val);
                }
            }
            if constexpr (std::is_same_v<T, int>)
            {
                if (auto *val = std::get_if<double>(&it->second))
                {
                    return static_cast<int>(*val);
                }
            }

            throw std::bad_variant_access();
        }

    private:
        std::unordered_map<std::string, ParamValue> m_values;
    };

} // namespace core

#endif // ALGORITHM_PARAMETERS_H