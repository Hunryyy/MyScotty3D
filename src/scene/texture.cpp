
#include "texture.h"

#include <iostream>

namespace Textures
{

	Spectrum sample_nearest(HDR_Image const &image, Vec2 uv)
	{
		// clamp texture coordinates, convert to [0,w]x[0,h] pixel space:
		float x = image.w * std::clamp(uv.x, 0.0f, 1.0f);
		float y = image.h * std::clamp(uv.y, 0.0f, 1.0f);

		// the pixel with the nearest center is the pixel that contains (x,y):
		int32_t ix = int32_t(std::floor(x));
		int32_t iy = int32_t(std::floor(y));

		// texture coordinates of (1,1) map to (w,h), and need to be reduced:
		ix = std::min(ix, int32_t(image.w) - 1);
		iy = std::min(iy, int32_t(image.h) - 1);

		return image.at(ix, iy);
	}

	Spectrum sample_bilinear(HDR_Image const &image, Vec2 uv)
	{
		// A1T6: sample_bilinear
		// TODO: implement bilinear sampling strategy on texture 'image'
		float x = image.w * uv.x;
		float y = image.h * uv.y;

		// 2. 找到采样中心。在 Scotty3D 中，像素中心位于 (int + 0.5)
		// 我们减去 0.5 使最近的像素左上角对齐到整数索引
		float ux = x - 0.5f;
		float uy = y - 0.5f;

		int32_t x0 = static_cast<int32_t>(std::floor(ux));
		int32_t y0 = static_cast<int32_t>(std::floor(uy));

		// 3. 计算插值权重 (u, v)
		float s = ux - std::floor(ux);
		float t = uy - std::floor(uy);

		// 4. 获取四个相邻像素的颜色（注意边界处理，使用 clamp 或 wrap）
		auto get_pixel = [&](int32_t ix, int32_t iy)
		{
			ix = std::clamp(ix, 0, (int32_t)image.w - 1);
			iy = std::clamp(iy, 0, (int32_t)image.h - 1);
			return image.at(ix, iy);
		};

		Spectrum c00 = get_pixel(x0, y0);
		Spectrum c10 = get_pixel(x0 + 1, y0);
		Spectrum c01 = get_pixel(x0, y0 + 1);
		Spectrum c11 = get_pixel(x0 + 1, y0 + 1);

		// 5. 进行双线性插值
		return (1.0f - s) * (1.0f - t) * c00 +
			   s * (1.0f - t) * c10 +
			   (1.0f - s) * t * c01 +
			   s * t * c11;
	}

	Spectrum sample_trilinear(HDR_Image const &base, std::vector<HDR_Image> const &levels, Vec2 uv, float lod)
	{
		// A1T6: sample_trilinear
		// TODO: implement trilinear sampling strategy on using mip-map 'levels'
		// 1. 处理边界情况：LOD <= 0 使用 base 层的双线性采样
		if (lod <= 0.0f)
			return sample_bilinear(base, uv);

		// 2. 处理边界情况：LOD 过大使用最高层（1x1）
		if (lod >= static_cast<float>(levels.size()))
		{
			return sample_bilinear(levels.back(), uv);
		}

		// 3. 确定插值的两个层级
		// 假设 lod = 0.5，则在 base 层和 levels[0] 之间插值
		int32_t level_i = static_cast<int32_t>(std::floor(lod));
		float t = lod - std::floor(lod);

		Spectrum c_low, c_high;

		if (level_i == 0)
		{
			c_low = sample_bilinear(base, uv);
			c_high = sample_bilinear(levels[0], uv);
		}
		else
		{
			c_low = sample_bilinear(levels[level_i - 1], uv);
			c_high = sample_bilinear(levels[level_i], uv);
		}

		// 4. 层级间线性插值
		return (1.0f - t) * c_low + t * c_high;
	}

	/*
	 * generate_mipmap- generate mipmap levels from a base image.
	 *  base: the base image
	 *  levels: pointer to vector of levels to fill (must not be null)
	 *
	 * generates a stack of levels [1,n] of sizes w_i, h_i, where:
	 *   w_i = max(1, floor(w_{i-1})/2)
	 *   h_i = max(1, floor(h_{i-1})/2)
	 *  with:
	 *   w_0 = base.w
	 *   h_0 = base.h
	 *  and n is the smalles n such that w_n = h_n = 1
	 *
	 * each level should be calculated by downsampling a blurred version
	 * of the previous level to remove high-frequency detail.
	 *
	 */
	void generate_mipmap(HDR_Image const &base, std::vector<HDR_Image> *levels_)
	{
		assert(levels_);
		auto &levels = *levels_;

		{ // allocate sublevels sufficient to scale base image all the way to 1x1:
			int32_t num_levels = static_cast<int32_t>(std::log2(std::max(base.w, base.h)));
			assert(num_levels >= 0);

			levels.clear();
			levels.reserve(num_levels);

			uint32_t width = base.w;
			uint32_t height = base.h;
			for (int32_t i = 0; i < num_levels; ++i)
			{
				assert(!(width == 1 && height == 1)); // would have stopped before this if num_levels was computed correctly

				width = std::max(1u, width / 2u);
				height = std::max(1u, height / 2u);

				levels.emplace_back(width, height);
			}
			assert(width == 1 && height == 1);
			assert(levels.size() == uint32_t(num_levels));
		}

		// now fill in the levels using a helper:
		// downsample:
		//  fill in dst to represent the low-frequency component of src
		auto downsample = [](HDR_Image const &src, HDR_Image &dst)
		{
			// dst is half the size of src in each dimension:
			assert(std::max(1u, src.w / 2u) == dst.w);
			assert(std::max(1u, src.h / 2u) == dst.h);

			// A1T6: generate
			// TODO: Write code to fill the levels of the mipmap hierarchy by downsampling

			// Be aware that the alignment of the samples in dst and src will be different depending on whether the image is even or odd.
			for (uint32_t y = 0; y < dst.h; ++y)
			{
				for (uint32_t x = 0; x < dst.w; ++x)
				{
					// 计算在 src 中对应的四个像素区域
					// 对于 2x2 到 1x1 的情况，需小心处理索引
					uint32_t src_x = x * 2;
					uint32_t src_y = y * 2;

					Spectrum sum = src.at(src_x, src_y);
					float count = 1.0f;

					if (src_x + 1 < src.w)
					{
						sum += src.at(src_x + 1, src_y);
						count += 1.0f;
					}
					if (src_y + 1 < src.h)
					{
						sum += src.at(src_x, src_y + 1);
						count += 1.0f;
					}
					if (src_x + 1 < src.w && src_y + 1 < src.h)
					{
						sum += src.at(src_x + 1, src_y + 1);
						count += 1.0f;
					}

					dst.at(x, y) = sum * (1.0f / count);
				}
			}
		};

		std::cout << "Regenerating mipmap (" << levels.size() << " levels): [" << base.w << "x" << base.h << "]";
		std::cout.flush();
		for (uint32_t i = 0; i < levels.size(); ++i)
		{
			HDR_Image const &src = (i == 0 ? base : levels[i - 1]);
			HDR_Image &dst = levels[i];
			std::cout << " -> [" << dst.w << "x" << dst.h << "]";
			std::cout.flush();

			downsample(src, dst);
		}
		std::cout << std::endl;
	}

	Image::Image(Sampler sampler_, HDR_Image const &image_)
	{
		sampler = sampler_;
		image = image_.copy();
		update_mipmap();
	}

	Spectrum Image::evaluate(Vec2 uv, float lod) const
	{
		if (image.w == 0 && image.h == 0)
			return Spectrum();
		if (sampler == Sampler::nearest)
		{
			return sample_nearest(image, uv);
		}
		else if (sampler == Sampler::bilinear)
		{
			return sample_bilinear(image, uv);
		}
		else
		{
			return sample_trilinear(image, levels, uv, lod);
		}
	}

	void Image::update_mipmap()
	{
		if (sampler == Sampler::trilinear)
		{
			generate_mipmap(image, &levels);
		}
		else
		{
			levels.clear();
		}
	}

	GL::Tex2D Image::to_gl() const
	{
		return image.to_gl(1.0f);
	}

	void Image::make_valid()
	{
		update_mipmap();
	}

	Spectrum Constant::evaluate(Vec2 uv, float lod) const
	{
		return color * scale;
	}

} // namespace Textures
bool operator!=(const Textures::Constant &a, const Textures::Constant &b)
{
	return a.color != b.color || a.scale != b.scale;
}

bool operator!=(const Textures::Image &a, const Textures::Image &b)
{
	return a.image != b.image;
}

bool operator!=(const Texture &a, const Texture &b)
{
	if (a.texture.index() != b.texture.index())
		return false;
	return std::visit(
		[&](const auto &data)
		{ return data != std::get<std::decay_t<decltype(data)>>(b.texture); },
		a.texture);
}
