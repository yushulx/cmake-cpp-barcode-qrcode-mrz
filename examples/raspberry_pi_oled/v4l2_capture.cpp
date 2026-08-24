// A minimal V4L2 (Video4Linux2) camera capture class.
// Streams YUYV frames via memory-mapped buffers and converts them to RGB888,
// so the program does not depend on OpenCV or any other imaging framework.

#include "v4l2_capture.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#define V4L2_BUFFER_COUNT 4

static int xioctl(int fd, int request, void *arg)
{
	int ret;
	do
	{
		ret = ioctl(fd, request, arg);
	} while (ret == -1 && errno == EINTR);
	return ret;
}

V4L2Capture::V4L2Capture() : fd_(-1), width_(0), height_(0)
{
}

V4L2Capture::~V4L2Capture()
{
	close();
}

bool V4L2Capture::open(const char *device, int width, int height)
{
	fd_ = ::open(device, O_RDWR | O_NONBLOCK, 0);
	if (fd_ < 0)
	{
		std::cerr << "ERROR: cannot open " << device << ": " << strerror(errno) << std::endl;
		return false;
	}

	// Query the capture capabilities.
	v4l2_capability cap;
	memset(&cap, 0, sizeof(cap));
	if (xioctl(fd_, VIDIOC_QUERYCAP, &cap) == -1)
	{
		std::cerr << "ERROR: VIDIOC_QUERYCAP failed: " << strerror(errno) << std::endl;
		close();
		return false;
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		std::cerr << "ERROR: " << device << " is not a video capture device" << std::endl;
		close();
		return false;
	}
	if (!(cap.capabilities & V4L2_CAP_STREAMING))
	{
		std::cerr << "ERROR: " << device << " does not support streaming I/O" << std::endl;
		close();
		return false;
	}

	// Set the pixel format to YUYV and the requested resolution.
	v4l2_format fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	if (xioctl(fd_, VIDIOC_S_FMT, &fmt) == -1)
	{
		std::cerr << "ERROR: VIDIOC_S_FMT failed: " << strerror(errno) << std::endl;
		close();
		return false;
	}
	width_ = fmt.fmt.pix.width;
	height_ = fmt.fmt.pix.height;

	// Request memory-mapped buffers.
	v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
	req.count = V4L2_BUFFER_COUNT;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (xioctl(fd_, VIDIOC_REQBUFS, &req) == -1)
	{
		std::cerr << "ERROR: VIDIOC_REQBUFS failed: " << strerror(errno) << std::endl;
		close();
		return false;
	}
	if (req.count < 2)
	{
		std::cerr << "ERROR: insufficient buffer memory on " << device << std::endl;
		close();
		return false;
	}

	for (unsigned int i = 0; i < req.count; ++i)
	{
		v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (xioctl(fd_, VIDIOC_QUERYBUF, &buf) == -1)
		{
			std::cerr << "ERROR: VIDIOC_QUERYBUF failed: " << strerror(errno) << std::endl;
			close();
			return false;
		}
		void *mapped = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buf.m.offset);
		if (mapped == MAP_FAILED)
		{
			std::cerr << "ERROR: mmap failed: " << strerror(errno) << std::endl;
			close();
			return false;
		}
		buffers_.push_back(mapped);
		bufferSizes_.push_back(buf.length);

		if (xioctl(fd_, VIDIOC_QBUF, &buf) == -1)
		{
			std::cerr << "ERROR: VIDIOC_QBUF failed: " << strerror(errno) << std::endl;
			close();
			return false;
		}
	}

	// Start the video stream.
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd_, VIDIOC_STREAMON, &type) == -1)
	{
		std::cerr << "ERROR: VIDIOC_STREAMON failed: " << strerror(errno) << std::endl;
		close();
		return false;
	}

	return true;
}

// Converts one YUYV pixel pair (Y0 U Y1 V) into two RGB888 pixels (BT.601).
static void yuyv_to_rgb888(const unsigned char *yuyv, unsigned char *rgb, int width, int height)
{
	int pixelCount = width * height;
	for (int i = 0; i < pixelCount; i += 2)
	{
		unsigned char y0 = yuyv[0];
		unsigned char u = yuyv[1];
		unsigned char y1 = yuyv[2];
		unsigned char v = yuyv[3];
		yuyv += 4;

		int c0 = y0 - 16;
		int c1 = y1 - 16;
		int d = u - 128;
		int e = v - 128;

		int r0 = (298 * c0 + 409 * e + 128) >> 8;
		int g0 = (298 * c0 - 100 * d - 208 * e + 128) >> 8;
		int b0 = (298 * c0 + 516 * d + 128) >> 8;
		int r1 = (298 * c1 + 409 * e + 128) >> 8;
		int g1 = (298 * c1 - 100 * d - 208 * e + 128) >> 8;
		int b1 = (298 * c1 + 516 * d + 128) >> 8;

		rgb[0] = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
		rgb[1] = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
		rgb[2] = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);
		rgb[3] = r1 < 0 ? 0 : (r1 > 255 ? 255 : r1);
		rgb[4] = g1 < 0 ? 0 : (g1 > 255 ? 255 : g1);
		rgb[5] = b1 < 0 ? 0 : (b1 > 255 ? 255 : b1);
		rgb += 6;
	}
}

bool V4L2Capture::readFrame(std::vector<unsigned char> &rgb)
{
	v4l2_buffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (xioctl(fd_, VIDIOC_DQBUF, &buf) == -1)
	{
		if (errno == EAGAIN || errno == EINTR)
		{
			return false;
		}
		std::cerr << "ERROR: VIDIOC_DQBUF failed: " << strerror(errno) << std::endl;
		return false;
	}

	if ((int)buf.bytesused >= width_ * height_ * 2 && buf.index < buffers_.size())
	{
		rgb.resize(width_ * height_ * 3);
		yuyv_to_rgb888((const unsigned char *)buffers_[buf.index], rgb.data(), width_, height_);
	}

	if (xioctl(fd_, VIDIOC_QBUF, &buf) == -1)
	{
		std::cerr << "ERROR: VIDIOC_QBUF failed: " << strerror(errno) << std::endl;
		return false;
	}

	return true;
}

void V4L2Capture::close()
{
	if (fd_ != -1)
	{
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		xioctl(fd_, VIDIOC_STREAMOFF, &type);
		for (size_t i = 0; i < buffers_.size(); ++i)
		{
			if (buffers_[i] != NULL)
			{
				munmap(buffers_[i], bufferSizes_[i]);
			}
		}
		buffers_.clear();
		bufferSizes_.clear();
		::close(fd_);
		fd_ = -1;
	}
}
