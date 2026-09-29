#include "workload_visualizer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace workload_visualizer
{
    namespace
    {
        struct Color
        {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
        };

        constexpr Color white{255, 255, 255};
        constexpr Color black{20, 20, 20};
        constexpr Color grid_gray{220, 220, 220};
        constexpr Color bar_fill{80, 135, 200};
        constexpr Color point_fill{45, 100, 170};

        string trim_whitespace(const string &text)
        {
            size_t first_non_space = 0;
            while (first_non_space < text.size() &&
                   isspace(static_cast<unsigned char>(text[first_non_space])))
            {
                ++first_non_space;
            }

            size_t one_past_last_non_space = text.size();
            while (one_past_last_non_space > first_non_space &&
                   isspace(static_cast<unsigned char>(text[one_past_last_non_space - 1])))
            {
                --one_past_last_non_space;
            }

            return text.substr(first_non_space,
                               one_past_last_non_space - first_non_space);
        }

        string normalize_name(const string &text)
        {
            string normalized;
            for (unsigned char character : text)
            {
                if (isalnum(character))
                {
                    normalized += static_cast<char>(tolower(character));
                }
            }
            return normalized;
        }

        long long parse_integer(const string &text, const string &description)
        {
            const string trimmed_text = trim_whitespace(text);
            size_t characters_used = 0;
            long long parsed_value = 0;

            try
            {
                parsed_value = stoll(trimmed_text, &characters_used);
            }
            catch (const exception &)
            {
                throw runtime_error(description + " must be an integer: " + text);
            }

            if (characters_used != trimmed_text.size())
            {
                throw runtime_error(description + " must be an integer: " + text);
            }

            return parsed_value;
        }

        size_t count_tasks(const string &task_list_text)
        {
            const string trimmed_text = trim_whitespace(task_list_text);
            if (trimmed_text == "[]")
            {
                return 0;
            }

            if (trimmed_text.size() < 2 || trimmed_text.front() != '[' || trimmed_text.back() != ']')
            {
                throw runtime_error("Tasks must be a bracketed list: " + task_list_text);
            }

            const string inside_list = trimmed_text.substr(1, trimmed_text.size() - 2);
            if (trim_whitespace(inside_list).empty())
            {
                return 0;
            }

            size_t task_count = 1;
            char active_quote = '\0';

            for (size_t character_index = 0; character_index < inside_list.size(); ++character_index)
            {
                const char character = inside_list[character_index];

                if (active_quote != '\0')
                {
                    if (character == active_quote)
                    {
                        active_quote = '\0';
                    }
                }
                else if (character == '\'' || character == '\"')
                {
                    active_quote = character;
                }
                else if (character == ',')
                {
                    ++task_count;
                }
            }

            if (active_quote != '\0')
            {
                throw runtime_error("unterminated quote in Tasks field: " + task_list_text);
            }

            return task_count;
        }

        size_t find_required_column(const map<string, size_t> &header_indexes,
                                    const vector<string> &accepted_names,
                                    const string &description)
        {
            for (const string &accepted_name : accepted_names)
            {
                const auto found_column = header_indexes.find(accepted_name);
                if (found_column != header_indexes.end())
                {
                    return found_column->second;
                }
            }

            throw runtime_error("missing required workload column: " + description);
        }

        // A small RGB canvas is enough for the two plots required by Obj-A.
        class ImageCanvas
        {
        public:
            ImageCanvas(size_t image_width, size_t image_height, Color background)
                : width(image_width),
                  height(image_height),
                  pixels(image_width * image_height * 3, 0)
            {
                fill(background);
            }

            size_t get_width() const
            {
                return width;
            }

            size_t get_height() const
            {
                return height;
            }

            const vector<uint8_t> &get_pixels() const
            {
                return pixels;
            }

            void fill(Color color)
            {
                for (size_t pixel_index = 0; pixel_index < width * height; ++pixel_index)
                {
                    pixels[pixel_index * 3] = color.red;
                    pixels[pixel_index * 3 + 1] = color.green;
                    pixels[pixel_index * 3 + 2] = color.blue;
                }
            }

            void set_pixel(int x_coordinate, int y_coordinate, Color color)
            {
                if (x_coordinate < 0 || y_coordinate < 0 ||
                    x_coordinate >= static_cast<int>(width) ||
                    y_coordinate >= static_cast<int>(height))
                {
                    return;
                }

                const size_t pixel_index =
                    (static_cast<size_t>(y_coordinate) * width +
                     static_cast<size_t>(x_coordinate)) *
                    3;

                pixels[pixel_index] = color.red;
                pixels[pixel_index + 1] = color.green;
                pixels[pixel_index + 2] = color.blue;
            }

            void draw_line(int start_x, int start_y,
                           int end_x, int end_y,
                           Color color)
            {
                const int x_distance = abs(end_x - start_x);
                const int x_step = start_x < end_x ? 1 : -1;
                const int y_distance = -abs(end_y - start_y);
                const int y_step = start_y < end_y ? 1 : -1;
                int error_value = x_distance + y_distance;

                int current_x = start_x;
                int current_y = start_y;

                while (true)
                {
                    set_pixel(current_x, current_y, color);
                    if (current_x == end_x && current_y == end_y)
                    {
                        break;
                    }

                    const int doubled_error = 2 * error_value;
                    if (doubled_error >= y_distance)
                    {
                        error_value += y_distance;
                        current_x += x_step;
                    }
                    if (doubled_error <= x_distance)
                    {
                        error_value += x_distance;
                        current_y += y_step;
                    }
                }
            }

            void fill_rectangle(int left, int top, int right, int bottom, Color color)
            {
                if (right < left)
                {
                    swap(left, right);
                }
                if (bottom < top)
                {
                    swap(top, bottom);
                }

                for (int y_coordinate = top; y_coordinate <= bottom; ++y_coordinate)
                {
                    for (int x_coordinate = left; x_coordinate <= right; ++x_coordinate)
                    {
                        set_pixel(x_coordinate, y_coordinate, color);
                    }
                }
            }

            void draw_rectangle(int left, int top, int right, int bottom, Color color)
            {
                draw_line(left, top, right, top, color);
                draw_line(right, top, right, bottom, color);
                draw_line(right, bottom, left, bottom, color);
                draw_line(left, bottom, left, top, color);
            }

            void fill_circle(int center_x, int center_y, int radius, Color color)
            {
                for (int y_offset = -radius; y_offset <= radius; ++y_offset)
                {
                    for (int x_offset = -radius; x_offset <= radius; ++x_offset)
                    {
                        if (x_offset * x_offset + y_offset * y_offset <= radius * radius)
                        {
                            set_pixel(center_x + x_offset,
                                      center_y + y_offset,
                                      color);
                        }
                    }
                }
            }

            void draw_text(int start_x, int start_y,
                           const string &text,
                           int scale,
                           Color color)
            {
                int current_x = start_x;

                for (char character : text)
                {
                    const array<uint8_t, 5> columns = glyph_columns(character);

                    for (size_t column_index = 0; column_index < columns.size(); ++column_index)
                    {
                        for (int row_index = 0; row_index < 7; ++row_index)
                        {
                            if ((columns[column_index] & (1U << row_index)) != 0)
                            {
                                fill_rectangle(current_x + static_cast<int>(column_index) * scale,
                                               start_y + row_index * scale,
                                               current_x + static_cast<int>(column_index + 1) * scale - 1,
                                               start_y + (row_index + 1) * scale - 1,
                                               color);
                            }
                        }
                    }

                    current_x += 6 * scale;
                }
            }

            int text_width(const string &text, int scale) const
            {
                return static_cast<int>(text.size()) * 6 * scale;
            }

        private:
            size_t width;
            size_t height;
            vector<uint8_t> pixels;

            static array<uint8_t, 5> glyph_columns(char input_character)
            {
                const char character = static_cast<char>(toupper(static_cast<unsigned char>(input_character)));

                switch (character)
                {
                case 'A':
                    return {0x7E, 0x11, 0x11, 0x11, 0x7E};
                case 'B':
                    return {0x7F, 0x49, 0x49, 0x49, 0x36};
                case 'C':
                    return {0x3E, 0x41, 0x41, 0x41, 0x22};
                case 'D':
                    return {0x7F, 0x41, 0x41, 0x22, 0x1C};
                case 'E':
                    return {0x7F, 0x49, 0x49, 0x49, 0x41};
                case 'F':
                    return {0x7F, 0x09, 0x09, 0x09, 0x01};
                case 'G':
                    return {0x3E, 0x41, 0x49, 0x49, 0x7A};
                case 'H':
                    return {0x7F, 0x08, 0x08, 0x08, 0x7F};
                case 'I':
                    return {0x00, 0x41, 0x7F, 0x41, 0x00};
                case 'J':
                    return {0x20, 0x40, 0x41, 0x3F, 0x01};
                case 'K':
                    return {0x7F, 0x08, 0x14, 0x22, 0x41};
                case 'L':
                    return {0x7F, 0x40, 0x40, 0x40, 0x40};
                case 'M':
                    return {0x7F, 0x02, 0x0C, 0x02, 0x7F};
                case 'N':
                    return {0x7F, 0x04, 0x08, 0x10, 0x7F};
                case 'O':
                    return {0x3E, 0x41, 0x41, 0x41, 0x3E};
                case 'P':
                    return {0x7F, 0x09, 0x09, 0x09, 0x06};
                case 'Q':
                    return {0x3E, 0x41, 0x51, 0x21, 0x5E};
                case 'R':
                    return {0x7F, 0x09, 0x19, 0x29, 0x46};
                case 'S':
                    return {0x46, 0x49, 0x49, 0x49, 0x31};
                case 'T':
                    return {0x01, 0x01, 0x7F, 0x01, 0x01};
                case 'U':
                    return {0x3F, 0x40, 0x40, 0x40, 0x3F};
                case 'V':
                    return {0x1F, 0x20, 0x40, 0x20, 0x1F};
                case 'W':
                    return {0x3F, 0x40, 0x38, 0x40, 0x3F};
                case 'X':
                    return {0x63, 0x14, 0x08, 0x14, 0x63};
                case 'Y':
                    return {0x07, 0x08, 0x70, 0x08, 0x07};
                case 'Z':
                    return {0x61, 0x51, 0x49, 0x45, 0x43};
                case '0':
                    return {0x3E, 0x51, 0x49, 0x45, 0x3E};
                case '1':
                    return {0x00, 0x42, 0x7F, 0x40, 0x00};
                case '2':
                    return {0x42, 0x61, 0x51, 0x49, 0x46};
                case '3':
                    return {0x21, 0x41, 0x45, 0x4B, 0x31};
                case '4':
                    return {0x18, 0x14, 0x12, 0x7F, 0x10};
                case '5':
                    return {0x27, 0x45, 0x45, 0x45, 0x39};
                case '6':
                    return {0x3C, 0x4A, 0x49, 0x49, 0x30};
                case '7':
                    return {0x01, 0x71, 0x09, 0x05, 0x03};
                case '8':
                    return {0x36, 0x49, 0x49, 0x49, 0x36};
                case '9':
                    return {0x06, 0x49, 0x49, 0x29, 0x1E};
                case '-':
                    return {0x08, 0x08, 0x08, 0x08, 0x08};
                case '.':
                    return {0x00, 0x60, 0x60, 0x00, 0x00};
                case ',':
                    return {0x00, 0x40, 0x20, 0x00, 0x00};
                case ':':
                    return {0x00, 0x36, 0x36, 0x00, 0x00};
                case '(':
                    return {0x00, 0x1C, 0x22, 0x41, 0x00};
                case ')':
                    return {0x00, 0x41, 0x22, 0x1C, 0x00};
                case '/':
                    return {0x20, 0x10, 0x08, 0x04, 0x02};
                case '_':
                    return {0x40, 0x40, 0x40, 0x40, 0x40};
                case '+':
                    return {0x08, 0x08, 0x3E, 0x08, 0x08};
                case ' ':
                    return {0x00, 0x00, 0x00, 0x00, 0x00};
                default:
                    return {0x02, 0x01, 0x51, 0x09, 0x06};
                }
            }
        };

        uint32_t update_crc32(uint32_t current_crc, uint8_t byte_value)
        {
            current_crc ^= byte_value;
            for (int bit_number = 0; bit_number < 8; ++bit_number)
            {
                if ((current_crc & 1U) != 0)
                {
                    current_crc = (current_crc >> 1U) ^ 0xEDB88320U;
                }
                else
                {
                    current_crc >>= 1U;
                }
            }
            return current_crc;
        }

        uint32_t calculate_crc32(const string &chunk_type, const vector<uint8_t> &chunk_data)
        {
            uint32_t crc_value = 0xFFFFFFFFU;

            for (unsigned char character : chunk_type)
            {
                crc_value = update_crc32(crc_value, character);
            }
            for (uint8_t byte_value : chunk_data)
            {
                crc_value = update_crc32(crc_value, byte_value);
            }

            return crc_value ^ 0xFFFFFFFFU;
        }

        uint32_t calculate_adler32(const vector<uint8_t> &data)
        {
            constexpr uint32_t modulus = 65521U;
            uint32_t first_sum = 1U;
            uint32_t second_sum = 0U;

            for (uint8_t byte_value : data)
            {
                first_sum = (first_sum + byte_value) % modulus;
                second_sum = (second_sum + first_sum) % modulus;
            }

            return (second_sum << 16U) | first_sum;
        }

        void append_uint32_big_endian(vector<uint8_t> &output_bytes, uint32_t value)
        {
            output_bytes.push_back(static_cast<uint8_t>((value >> 24U) & 0xFFU));
            output_bytes.push_back(static_cast<uint8_t>((value >> 16U) & 0xFFU));
            output_bytes.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
            output_bytes.push_back(static_cast<uint8_t>(value & 0xFFU));
        }

        void append_png_chunk(vector<uint8_t> &png_bytes,
                              const string &chunk_type,
                              const vector<uint8_t> &chunk_data)
        {
            append_uint32_big_endian(png_bytes,
                                     static_cast<uint32_t>(chunk_data.size()));

            for (char character : chunk_type)
            {
                png_bytes.push_back(static_cast<uint8_t>(character));
            }
            png_bytes.insert(png_bytes.end(), chunk_data.begin(), chunk_data.end());

            append_uint32_big_endian(png_bytes,
                                     calculate_crc32(chunk_type, chunk_data));
        }

        vector<uint8_t> make_uncompressed_zlib_stream(const vector<uint8_t> &raw_data)
        {
            vector<uint8_t> compressed_data;

            // 0x78 0x01 is a valid zlib header for a deflate stream with no compression.
            compressed_data.push_back(0x78);
            compressed_data.push_back(0x01);

            size_t next_byte = 0;
            while (next_byte < raw_data.size())
            {
                const size_t bytes_left = raw_data.size() - next_byte;
                const uint16_t block_length =
                    static_cast<uint16_t>(min<size_t>(bytes_left, 65535));
                const bool is_final_block = next_byte + block_length == raw_data.size();

                // Stored deflate blocks begin on a byte boundary.
                compressed_data.push_back(is_final_block ? 0x01 : 0x00);
                compressed_data.push_back(static_cast<uint8_t>(block_length & 0xFFU));
                compressed_data.push_back(static_cast<uint8_t>((block_length >> 8U) & 0xFFU));

                const uint16_t inverted_length = static_cast<uint16_t>(~block_length);
                compressed_data.push_back(static_cast<uint8_t>(inverted_length & 0xFFU));
                compressed_data.push_back(static_cast<uint8_t>((inverted_length >> 8U) & 0xFFU));

                compressed_data.insert(compressed_data.end(),
                                       raw_data.begin() + static_cast<ptrdiff_t>(next_byte),
                                       raw_data.begin() + static_cast<ptrdiff_t>(next_byte + block_length));
                next_byte += block_length;
            }

            const uint32_t adler_checksum = calculate_adler32(raw_data);
            append_uint32_big_endian(compressed_data, adler_checksum);
            return compressed_data;
        }

        void write_png_file(const filesystem::path &output_path, const ImageCanvas &image)
        {
            if (image.get_width() > numeric_limits<uint32_t>::max() ||
                image.get_height() > numeric_limits<uint32_t>::max())
            {
                throw runtime_error("image dimensions are too large for PNG");
            }

            vector<uint8_t> raw_scanlines;
            raw_scanlines.reserve((image.get_width() * 3 + 1) * image.get_height());

            const vector<uint8_t> &pixels = image.get_pixels();
            for (size_t y_coordinate = 0; y_coordinate < image.get_height(); ++y_coordinate)
            {
                // Filter type 0 leaves each row unchanged.
                raw_scanlines.push_back(0);
                const size_t row_start = y_coordinate * image.get_width() * 3;
                raw_scanlines.insert(raw_scanlines.end(),
                                     pixels.begin() + static_cast<ptrdiff_t>(row_start),
                                     pixels.begin() + static_cast<ptrdiff_t>(row_start + image.get_width() * 3));
            }

            vector<uint8_t> png_bytes = {137, 80, 78, 71, 13, 10, 26, 10};

            vector<uint8_t> header_data;
            append_uint32_big_endian(header_data, static_cast<uint32_t>(image.get_width()));
            append_uint32_big_endian(header_data, static_cast<uint32_t>(image.get_height()));
            header_data.push_back(8); // 8 bits per color channel.
            header_data.push_back(2); // Truecolor RGB.
            header_data.push_back(0); // Deflate compression.
            header_data.push_back(0); // Standard PNG filtering.
            header_data.push_back(0); // No interlacing.
            append_png_chunk(png_bytes, "IHDR", header_data);

            append_png_chunk(png_bytes, "IDAT", make_uncompressed_zlib_stream(raw_scanlines));
            append_png_chunk(png_bytes, "IEND", {});

            if (output_path.has_parent_path())
            {
                filesystem::create_directories(output_path.parent_path());
            }

            ofstream output_file(output_path, ios::binary | ios::trunc);
            if (!output_file)
            {
                throw runtime_error("cannot write PNG file: " + output_path.string());
            }

            output_file.write(reinterpret_cast<const char *>(png_bytes.data()),
                              static_cast<streamsize>(png_bytes.size()));
            if (!output_file)
            {
                throw runtime_error("failed while writing PNG file: " + output_path.string());
            }
        }

        struct AxisRange
        {
            double minimum;
            double maximum;
        };

        void validate_plot_options(const PlotOptions &plot_options)
        {
            if (plot_options.image_width < 400 || plot_options.image_height < 300)
            {
                throw invalid_argument("plot image must be at least 400x300 pixels");
            }

            const auto validate_limits = [](const optional<pair<double, double>> &limits,
                                            const string &axis_name)
            {
                if (!limits)
                {
                    return;
                }
                if (!isfinite(limits->first) || !isfinite(limits->second) ||
                    limits->second <= limits->first)
                {
                    throw invalid_argument(axis_name + " limits must be finite and written as min,max with max > min");
                }
            };

            validate_limits(plot_options.x_limits, "x-axis");
            validate_limits(plot_options.y_limits, "y-axis");
        }

        AxisRange automatic_range(const vector<double> &values,
                                  const optional<pair<double, double>> &requested_limits,
                                  bool include_zero)
        {
            if (requested_limits)
            {
                return {requested_limits->first, requested_limits->second};
            }
            if (values.empty())
            {
                throw invalid_argument("cannot plot an empty set of values");
            }

            const auto [minimum_iterator, maximum_iterator] =
                minmax_element(values.begin(), values.end());
            double minimum_value = *minimum_iterator;
            double maximum_value = *maximum_iterator;

            if (include_zero)
            {
                minimum_value = min(0.0, minimum_value);
                maximum_value = max(0.0, maximum_value);
            }

            if (minimum_value == maximum_value)
            {
                const double padding = minimum_value == 0.0
                                           ? 1.0
                                           : max(1.0, abs(minimum_value) * 0.1);
                minimum_value -= padding;
                maximum_value += padding;
            }
            else
            {
                const double padding = (maximum_value - minimum_value) * 0.05;
                minimum_value -= padding;
                maximum_value += padding;
            }

            return {minimum_value, maximum_value};
        }

        string format_number(double value)
        {
            ostringstream formatted_value;

            if (abs(value - round(value)) < 1e-9 && abs(value) < 1e12)
            {
                formatted_value << static_cast<long long>(llround(value));
            }
            else
            {
                formatted_value << fixed << setprecision(2) << value;
            }

            return formatted_value.str();
        }

        int map_x(double value, const AxisRange &range, int plot_left, int plot_right)
        {
            const double fraction = (value - range.minimum) / (range.maximum - range.minimum);
            return plot_left + static_cast<int>(llround(fraction * (plot_right - plot_left)));
        }

        int map_y(double value, const AxisRange &range, int plot_top, int plot_bottom)
        {
            const double fraction = (value - range.minimum) / (range.maximum - range.minimum);
            return plot_bottom - static_cast<int>(llround(fraction * (plot_bottom - plot_top)));
        }

        void draw_plot_frame(ImageCanvas &image,
                             const AxisRange &x_range,
                             const AxisRange &y_range,
                             const string &x_axis_label,
                             const string &y_axis_label,
                             const string &title,
                             int plot_left,
                             int plot_top,
                             int plot_right,
                             int plot_bottom)
        {
            constexpr int tick_count = 5;
            constexpr int small_text_scale = 2;
            constexpr int title_text_scale = 3;

            for (int tick_number = 0; tick_number <= tick_count; ++tick_number)
            {
                const double fraction = static_cast<double>(tick_number) / tick_count;
                const int x_coordinate = plot_left +
                                         static_cast<int>(llround(fraction * (plot_right - plot_left)));
                const int y_coordinate = plot_bottom -
                                         static_cast<int>(llround(fraction * (plot_bottom - plot_top)));

                image.draw_line(x_coordinate, plot_top, x_coordinate, plot_bottom, grid_gray);
                image.draw_line(plot_left, y_coordinate, plot_right, y_coordinate, grid_gray);

                const double x_value = x_range.minimum + fraction * (x_range.maximum - x_range.minimum);
                const double y_value = y_range.minimum + fraction * (y_range.maximum - y_range.minimum);

                const string x_tick_text = format_number(x_value);
                const string y_tick_text = format_number(y_value);

                image.draw_text(x_coordinate - image.text_width(x_tick_text, small_text_scale) / 2,
                                plot_bottom + 12,
                                x_tick_text,
                                small_text_scale,
                                black);
                image.draw_text(plot_left - image.text_width(y_tick_text, small_text_scale) - 12,
                                y_coordinate - 7,
                                y_tick_text,
                                small_text_scale,
                                black);
            }

            image.draw_rectangle(plot_left, plot_top, plot_right, plot_bottom, black);

            const string displayed_title = title.empty() ? "WORKLOAD PLOT" : title;
            image.draw_text(static_cast<int>(image.get_width()) / 2 -
                                image.text_width(displayed_title, title_text_scale) / 2,
                            25,
                            displayed_title,
                            title_text_scale,
                            black);

            image.draw_text(static_cast<int>(image.get_width()) / 2 -
                                image.text_width(x_axis_label, small_text_scale) / 2,
                            plot_bottom + 55,
                            x_axis_label,
                            small_text_scale,
                            black);

            image.draw_text(12,
                            55,
                            "Y: " + y_axis_label,
                            small_text_scale,
                            black);
        }

        void require_png_extension(const filesystem::path &output_path)
        {
            string extension = output_path.extension().string();
            transform(extension.begin(), extension.end(), extension.begin(),
                      [](unsigned char character)
                      {
                          return static_cast<char>(tolower(character));
                      });

            if (extension != ".png")
            {
                throw invalid_argument("visualizer output file must end in .png");
            }
        }
    }

    vector<string> parse_csv_line(const string &csv_line)
    {
        vector<string> parsed_fields;
        string current_field;
        bool inside_quoted_field = false;

        for (size_t character_index = 0; character_index < csv_line.size(); ++character_index)
        {
            const char current_character = csv_line[character_index];

            if (inside_quoted_field)
            {
                if (current_character == '\"')
                {
                    if (character_index + 1 < csv_line.size() &&
                        csv_line[character_index + 1] == '\"')
                    {
                        current_field.push_back('\"');
                        ++character_index;
                    }
                    else
                    {
                        inside_quoted_field = false;
                    }
                }
                else
                {
                    current_field.push_back(current_character);
                }
            }
            else if (current_character == '\"')
            {
                inside_quoted_field = true;
            }
            else if (current_character == ',')
            {
                parsed_fields.push_back(current_field);
                current_field.clear();
            }
            else if (current_character != '\r')
            {
                current_field.push_back(current_character);
            }
        }

        if (inside_quoted_field)
        {
            throw runtime_error("unterminated quoted CSV field");
        }

        parsed_fields.push_back(current_field);
        return parsed_fields;
    }

    vector<WorkloadRow> load_workload_file(const filesystem::path &workload_path)
    {
        ifstream workload_file(workload_path);
        if (!workload_file)
        {
            throw runtime_error("cannot open workload file: " + workload_path.string());
        }

        string current_line;
        if (!getline(workload_file, current_line))
        {
            throw runtime_error("workload file is empty: " + workload_path.string());
        }

        const vector<string> header_fields = parse_csv_line(current_line);
        map<string, size_t> header_indexes;
        for (size_t column_index = 0; column_index < header_fields.size(); ++column_index)
        {
            header_indexes[normalize_name(header_fields[column_index])] = column_index;
        }

        const size_t job_id_column =
            find_required_column(header_indexes, {"jobid", "id"}, "Job Id");
        const size_t arrival_time_column =
            find_required_column(header_indexes, {"arrivaltime", "arrival"}, "arrival time");
        const size_t tasks_column =
            find_required_column(header_indexes, {"tasks", "tasklist"}, "Tasks");

        const size_t largest_required_column =
            max({job_id_column, arrival_time_column, tasks_column});

        vector<WorkloadRow> workload_rows;
        size_t file_line_number = 1;

        while (getline(workload_file, current_line))
        {
            ++file_line_number;
            if (trim_whitespace(current_line).empty())
            {
                continue;
            }

            const vector<string> fields = parse_csv_line(current_line);
            if (fields.size() <= largest_required_column)
            {
                throw runtime_error("workload row " + to_string(file_line_number) +
                                    " does not contain all required columns");
            }

            WorkloadRow workload_row;
            workload_row.job_id = trim_whitespace(fields[job_id_column]);
            if (workload_row.job_id.empty())
            {
                throw runtime_error("Job Id cannot be empty on row " + to_string(file_line_number));
            }

            workload_row.arrival_time =
                parse_integer(fields[arrival_time_column], "arrival time on row " + to_string(file_line_number));
            workload_row.task_count = count_tasks(fields[tasks_column]);
            workload_rows.push_back(workload_row);
        }

        return workload_rows;
    }

    vector<double> numeric_column(const vector<WorkloadRow> &workload_rows,
                                  const string &column_name)
    {
        const string normalized_column_name = normalize_name(column_name);
        vector<double> values;
        values.reserve(workload_rows.size());

        if (normalized_column_name == "arrivaltime" || normalized_column_name == "arrival")
        {
            for (const WorkloadRow &row : workload_rows)
            {
                values.push_back(static_cast<double>(row.arrival_time));
            }
            return values;
        }

        if (normalized_column_name == "lentasks" ||
            normalized_column_name == "taskcount" ||
            normalized_column_name == "numberoftasks")
        {
            for (const WorkloadRow &row : workload_rows)
            {
                values.push_back(static_cast<double>(row.task_count));
            }
            return values;
        }

        throw invalid_argument("column is not numeric or is not supported for plotting: " + column_name);
    }

    void write_histogram_png(const filesystem::path &output_path,
                             const vector<double> &values,
                             size_t bin_count,
                             const string &x_axis_label,
                             const PlotOptions &plot_options)
    {
        require_png_extension(output_path);
        validate_plot_options(plot_options);

        if (values.empty())
        {
            throw invalid_argument("cannot create a histogram from an empty workload");
        }
        if (bin_count == 0)
        {
            throw invalid_argument("histogram bin count must be at least 1");
        }

        const AxisRange x_range = automatic_range(values, plot_options.x_limits, false);
        vector<size_t> bin_counts(bin_count, 0);

        for (double value : values)
        {
            if (value < x_range.minimum || value > x_range.maximum)
            {
                continue;
            }

            size_t bin_index = 0;
            if (value == x_range.maximum)
            {
                bin_index = bin_count - 1;
            }
            else
            {
                const double fraction =
                    (value - x_range.minimum) / (x_range.maximum - x_range.minimum);
                bin_index = min(bin_count - 1,
                                static_cast<size_t>(floor(fraction * bin_count)));
            }

            ++bin_counts[bin_index];
        }

        size_t maximum_count = 0;
        for (size_t bin_value : bin_counts)
        {
            maximum_count = max(maximum_count, bin_value);
        }

        AxisRange y_range;
        if (plot_options.y_limits)
        {
            y_range = {plot_options.y_limits->first, plot_options.y_limits->second};
        }
        else
        {
            const double automatic_maximum =
                static_cast<double>(max<size_t>(1, maximum_count)) * 1.05;
            y_range = {0.0, automatic_maximum};
        }

        ImageCanvas image(plot_options.image_width, plot_options.image_height, white);
        const int plot_left = 100;
        const int plot_top = 85;
        const int plot_right = static_cast<int>(plot_options.image_width) - 55;
        const int plot_bottom = static_cast<int>(plot_options.image_height) - 100;

        draw_plot_frame(image,
                        x_range,
                        y_range,
                        x_axis_label,
                        "JOB COUNT",
                        plot_options.title.empty() ? "HISTOGRAM" : plot_options.title,
                        plot_left,
                        plot_top,
                        plot_right,
                        plot_bottom);

        const double bin_width =
            (x_range.maximum - x_range.minimum) / static_cast<double>(bin_count);

        for (size_t bin_index = 0; bin_index < bin_count; ++bin_index)
        {
            const double bin_start_value = x_range.minimum + bin_index * bin_width;
            const double bin_end_value = bin_start_value + bin_width;
            const double count_value = static_cast<double>(bin_counts[bin_index]);
            if (count_value == 0.0)
            {
                continue;
            }

            int bar_left = map_x(bin_start_value, x_range, plot_left, plot_right) + 1;
            int bar_right = map_x(bin_end_value, x_range, plot_left, plot_right) - 1;
            int bar_top = map_y(min(count_value, y_range.maximum),
                                y_range,
                                plot_top,
                                plot_bottom);
            const int bar_bottom = map_y(max(0.0, y_range.minimum),
                                         y_range,
                                         plot_top,
                                         plot_bottom);

            bar_left = max(plot_left + 1, min(plot_right - 1, bar_left));
            bar_right = max(bar_left, min(plot_right - 1, bar_right));
            bar_top = max(plot_top + 1, min(plot_bottom - 1, bar_top));

            image.fill_rectangle(bar_left, bar_top, bar_right, bar_bottom, bar_fill);
            image.draw_rectangle(bar_left, bar_top, bar_right, bar_bottom, black);
        }

        write_png_file(output_path, image);
    }

    void write_scatter_png(const filesystem::path &output_path,
                           const vector<double> &x_values,
                           const vector<double> &y_values,
                           const string &x_axis_label,
                           const string &y_axis_label,
                           const PlotOptions &plot_options)
    {
        require_png_extension(output_path);
        validate_plot_options(plot_options);

        if (x_values.empty() || y_values.empty())
        {
            throw invalid_argument("cannot create a scatter plot from an empty workload");
        }
        if (x_values.size() != y_values.size())
        {
            throw invalid_argument("scatter plot x and y columns must have the same number of values");
        }

        const AxisRange x_range = automatic_range(x_values, plot_options.x_limits, false);
        const AxisRange y_range = automatic_range(y_values, plot_options.y_limits, false);

        ImageCanvas image(plot_options.image_width, plot_options.image_height, white);
        const int plot_left = 100;
        const int plot_top = 85;
        const int plot_right = static_cast<int>(plot_options.image_width) - 55;
        const int plot_bottom = static_cast<int>(plot_options.image_height) - 100;

        draw_plot_frame(image,
                        x_range,
                        y_range,
                        x_axis_label,
                        y_axis_label,
                        plot_options.title.empty() ? "SCATTER PLOT" : plot_options.title,
                        plot_left,
                        plot_top,
                        plot_right,
                        plot_bottom);

        for (size_t value_index = 0; value_index < x_values.size(); ++value_index)
        {
            const double x_value = x_values[value_index];
            const double y_value = y_values[value_index];

            if (x_value < x_range.minimum || x_value > x_range.maximum ||
                y_value < y_range.minimum || y_value > y_range.maximum)
            {
                continue;
            }

            image.fill_circle(map_x(x_value, x_range, plot_left, plot_right),
                              map_y(y_value, y_range, plot_top, plot_bottom),
                              4,
                              point_fill);
        }

        write_png_file(output_path, image);
    }

} // namespace workload_visualizer
