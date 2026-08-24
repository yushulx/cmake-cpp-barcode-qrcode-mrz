#ifndef V4L2_CAPTURE_H
#define V4L2_CAPTURE_H

#include <string>
#include <vector>

class V4L2Capture
{
public:
	V4L2Capture();
	~V4L2Capture();

	bool open(const char *device, int width, int height);
	void close();
	bool readFrame(std::vector<unsigned char> &rgb);
	int getWidth() const { return width_; }
	int getHeight() const { return height_; }

private:
	int fd_;
	int width_;
	int height_;
	std::vector<void *> buffers_;
	std::vector<size_t> bufferSizes_;
};

#endif
