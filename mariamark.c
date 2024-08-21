
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "mariamark.h"

#include <gst/video/gstvideometa.h>

GST_DEBUG_CATEGORY_STATIC (mariamark_debug);
#define GST_CAT_DEFAULT mariamark_debug

static void maria_mark_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);
static void maria_mark_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void maria_mark_finalize (GObject * object);

static gboolean maria_mark_start (GstBaseTransform * trans);
static gboolean maria_mark_stop (GstBaseTransform * trans);
static GstFlowReturn maria_mark_transform_frame_ip (GstVideoFilter * filter, GstVideoFrame * frame);
static void maria_mark_before_transform (GstBaseTransform * trans, GstBuffer * outbuf);
static gboolean maria_mark_set_info (GstVideoFilter * filter, GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static gboolean maria_mark_load_image (MariaMark * overlay, GError ** err);

enum
{
	PROP_0,
	PROP_LOCATION,
	PROP_OFFSET_X,
	PROP_OFFSET_Y,
	PROP_OVERLAY_WIDTH,
	PROP_OVERLAY_HEIGHT,
	PROP_ALPHA
};

#define VIDEO_FORMATS "{ RGBx, RGB, BGR, BGRx, xRGB, xBGR, " \
	"RGBA, BGRA, ARGB, ABGR, I420, YV12, AYUV, YUY2, UYVY, " \
	"v308, v210, v216, Y41B, Y42B, Y444, YVYU, NV12, NV21, UYVP, " \
	"RGB16, BGR16, RGB15, BGR15, UYVP, A420, YUV9, YVU9, " \
	"IYU1, ARGB64, AYUV64, r210, I420_10LE, I420_10BE, " \
	"GRAY8, GRAY16_BE, GRAY16_LE }"

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (VIDEO_FORMATS)));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (VIDEO_FORMATS)));

G_DEFINE_TYPE (MariaMark, maria_mark, GST_TYPE_VIDEO_FILTER);

static void maria_mark_class_init (MariaMarkClass * klass)
{
	GstVideoFilterClass *videofilter_class = GST_VIDEO_FILTER_CLASS (klass);
	GstBaseTransformClass *basetrans_class = GST_BASE_TRANSFORM_CLASS (klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = maria_mark_set_property;
	gobject_class->get_property = maria_mark_get_property;
	gobject_class->finalize = maria_mark_finalize;

	basetrans_class->start = GST_DEBUG_FUNCPTR (maria_mark_start);
	basetrans_class->stop = GST_DEBUG_FUNCPTR (maria_mark_stop);

	basetrans_class->before_transform = GST_DEBUG_FUNCPTR (maria_mark_before_transform);

	videofilter_class->set_info = GST_DEBUG_FUNCPTR (maria_mark_set_info);
	videofilter_class->transform_frame_ip = GST_DEBUG_FUNCPTR (maria_mark_transform_frame_ip);

	g_object_class_install_property (gobject_class, PROP_LOCATION, g_param_spec_string ("location", "location", "Location of image file to overlay", NULL, GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_OFFSET_X, g_param_spec_int ("offset-x", "X Offset", "For positive value, horizontal offset of overlay image in pixels from"
          " left of video image. For negative value, horizontal offset of overlay"
          " image in pixels from right of video image", G_MININT, G_MAXINT, 0, GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_OFFSET_Y, g_param_spec_int ("offset-y", "Y Offset", "For positive value, vertical offset of overlay image in pixels from"
          " top of video image. For negative value, vertical offset of overlay"
          " image in pixels from bottom of video image", G_MININT, G_MAXINT, 0, GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_OVERLAY_WIDTH, g_param_spec_int ("overlay-width", "Overlay Width", "Width of overlay image in pixels (0 = same as overlay image)", 0, G_MAXINT, 0, GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_OVERLAY_HEIGHT, g_param_spec_int ("overlay-height", "Overlay Height", "Height of overlay image in pixels (0 = same as overlay image)", 0, G_MAXINT, 0, GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_ALPHA, g_param_spec_double ("alpha", "Alpha", "Global alpha of overlay image", 0.0, 1.0, 1.0, GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&sink_template));
	gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&src_template));

	gst_element_class_set_static_metadata (element_class, "Maria Intel Water Mark", "Filter/Effect/Video", "Overlay an intel image onto a video stream", "Maria Joo <maria.joo@intel.com>");

	GST_DEBUG_CATEGORY_INIT (mariamark_debug, "mariamark", 0, "debug category for mariamark element");
}

static void maria_mark_init (MariaMark * overlay)
{
	overlay->offset_x = 0;
	overlay->offset_y = 0;

	overlay->overlay_width = 0;
	overlay->overlay_height = 0;

	overlay->alpha = 1.0;
}

void maria_mark_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
	MariaMark *overlay = MARIA_MARK (object);

	GST_OBJECT_LOCK (overlay);

	switch (property_id) {
		case PROP_LOCATION:
		{
			GError *err = NULL;
			g_free (overlay->location);
			overlay->location = g_value_dup_string (value);

			if (!maria_mark_load_image (overlay, &err))
			{
				GST_ERROR_OBJECT (overlay, "Could not load overlay image: %s",err->message);
 
				g_error_free (err);
			}
		}
			break;
		case PROP_OFFSET_X:
		{
			overlay->offset_x = g_value_get_int (value);
			overlay->update_composition = TRUE;
		}
      			break;
		case PROP_OFFSET_Y:
		{
			overlay->offset_y = g_value_get_int (value);
			overlay->update_composition = TRUE;
		}
			break;
		case PROP_OVERLAY_WIDTH:
		{
			overlay->overlay_width = g_value_get_int (value);
			overlay->update_composition = TRUE;
		}
			break;
		case PROP_OVERLAY_HEIGHT:
		{
			overlay->overlay_height = g_value_get_int (value);
			overlay->update_composition = TRUE;
		}
			break;
		case PROP_ALPHA:
		{
			overlay->alpha = g_value_get_double (value);
			overlay->update_composition = TRUE;
		}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}

	GST_OBJECT_UNLOCK (overlay);
}

void maria_mark_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
       MariaMark *overlay = MARIA_MARK (object);

	GST_OBJECT_LOCK (overlay);

	switch (property_id)
	{
		case PROP_LOCATION:
		{
			g_value_set_string (value, overlay->location);
		}
			break;
		case PROP_OFFSET_X:
		{
			g_value_set_int (value, overlay->offset_x);
		}
			break;
		case PROP_OFFSET_Y:
		{
			g_value_set_int (value, overlay->offset_y);
		}
			break;
		case PROP_OVERLAY_WIDTH:
		{
			g_value_set_int (value, overlay->overlay_width);
		}
			break;
		case PROP_OVERLAY_HEIGHT:
		{
			g_value_set_int (value, overlay->overlay_height);
		}
			break;
		case PROP_ALPHA:
		{
			g_value_set_double (value, overlay->alpha);
		}
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}

	GST_OBJECT_UNLOCK (overlay);
}

void maria_mark_finalize (GObject * object)
{
	MariaMark *overlay = MARIA_MARK (object);

	g_free (overlay->location);
	overlay->location = NULL;

	G_OBJECT_CLASS (maria_mark_parent_class)->finalize (object);
}

static gboolean maria_mark_load_image (MariaMark * overlay, GError ** err)
{
	GstVideoMeta *video_meta;
	GdkPixbuf *pixbuf;
	guint8 *pixels, *p;
	gint width, height, stride, w, h;

	pixbuf = gdk_pixbuf_new_from_file (overlay->location, err);

	if (pixbuf == NULL)
	{
		return FALSE;
	}

	if (!gdk_pixbuf_get_has_alpha (pixbuf))
	{
		GdkPixbuf *alpha_pixbuf;

		alpha_pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
		g_object_unref (pixbuf);
		pixbuf = alpha_pixbuf;
	}

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	stride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

 //* the memory layout in GdkPixbuf is R-G-B-A, we want:
 //  *  - B-G-R-A on little-endian platforms
   
	for (h = 0; h < height; ++h)
	{
		p = pixels + (h * stride );
		for (w = 0; w < width; ++w)
		{
			guint8 tmp;

      // R-G-B-A ==> B-G-R-A 
			tmp = p[0];
			p[0] = p[2];
			p[2] = tmp;
	
			p += 4;
		}
	}

  // assume we have row padding even for the last row */
  // transfer ownership of pixbuf to the buffer */
	overlay->pixels = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, pixels, height * stride, 0, height * stride, pixbuf, (GDestroyNotify) g_object_unref);
	video_meta = gst_buffer_add_video_meta (overlay->pixels, GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, width, height);

	overlay->update_composition = TRUE;

	GST_INFO_OBJECT (overlay, "Loaded image, %d x %d", width, height);

	return TRUE;
}

static gboolean maria_mark_start (GstBaseTransform * trans)
{
	MariaMark *overlay = MARIA_MARK (trans);
	GError *err = NULL;

	if (overlay->location != NULL)
	{
		if (!maria_mark_load_image (overlay, &err))
		{
			GST_ELEMENT_ERROR (overlay, RESOURCE, OPEN_READ, ("Could not load overlay image."), ("%s", err->message));
			g_error_free (err);

			return FALSE;
		}
		
		gst_base_transform_set_passthrough (trans, FALSE);
	} else {
		GST_WARNING_OBJECT (overlay, "no image location set, doing nothing");
		gst_base_transform_set_passthrough (trans, TRUE);
	}

	return TRUE;
}

static gboolean maria_mark_stop (GstBaseTransform * trans)
{
	MariaMark *overlay = MARIA_MARK (trans);

	if (overlay->comp)
	{
		gst_video_overlay_composition_unref (overlay->comp);
		overlay->comp = NULL;
	}

	gst_buffer_replace (&overlay->pixels, NULL);

	return TRUE;
}

static gboolean maria_mark_set_info (GstVideoFilter * filter, GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
	GST_INFO_OBJECT (filter, "caps: %" GST_PTR_FORMAT, incaps);

	return TRUE;
}

static void maria_mark_update_composition (MariaMark * overlay)
{
	GstVideoOverlayComposition *comp;
	GstVideoOverlayRectangle *rect;
	GstVideoMeta *overlay_meta;
	gint x, y, width, height;
	gint video_width = GST_VIDEO_INFO_WIDTH (&GST_VIDEO_FILTER (overlay)->in_info);
	gint video_height = GST_VIDEO_INFO_HEIGHT (&GST_VIDEO_FILTER (overlay)->in_info);

	if (overlay->comp)
	{
		gst_video_overlay_composition_unref (overlay->comp);
		overlay->comp = NULL;
	}

	if (overlay->alpha == 0.0 || overlay->pixels == NULL)
	{
		return;
	}
	overlay_meta = gst_buffer_get_video_meta (overlay->pixels);

	x = overlay->offset_x < 0 ? video_width + overlay->offset_x - overlay_meta->width : overlay->offset_x;
	y = overlay->offset_y < 0 ? video_height + overlay->offset_y - overlay_meta->height : overlay-> offset_y;

	width = overlay->overlay_width;

	if (width == 0)
	{
		width = overlay_meta->width;
	}
	height = overlay->overlay_height;

	if (height == 0)
	{
		height = overlay_meta->height;
	}

	GST_DEBUG_OBJECT (overlay, "overlay image dimensions: %d x %d, alpha=%.2f", overlay_meta->width, overlay_meta->height, overlay->alpha);
	GST_DEBUG_OBJECT (overlay, "properties: x,y: %d,%d - WxH: %dx%d", overlay->offset_x, overlay->offset_y, overlay->overlay_height, overlay->overlay_width);
	GST_DEBUG_OBJECT (overlay, "overlay rendered: %d x %d @ %d,%d (onto %d x %d)", width, height, x, y, video_width, video_height);

	rect = gst_video_overlay_rectangle_new_raw (overlay->pixels, x, y, width, height, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);

	if (overlay->alpha != 1.0)
	{
		gst_video_overlay_rectangle_set_global_alpha (rect, overlay->alpha);
	}
	comp = gst_video_overlay_composition_new (rect);
	gst_video_overlay_rectangle_unref (rect);

	overlay->comp = comp;
}

static void maria_mark_before_transform (GstBaseTransform * trans, GstBuffer * outbuf)
{
	GstClockTime stream_time;

	stream_time = gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME, GST_BUFFER_TIMESTAMP (outbuf));

	if (GST_CLOCK_TIME_IS_VALID (stream_time))
	{
		gst_object_sync_values (GST_OBJECT (trans), stream_time);
	}
}

static GstFlowReturn maria_mark_transform_frame_ip (GstVideoFilter * filter, GstVideoFrame * frame)
{
	MariaMark *overlay = MARIA_MARK (filter);

	GST_OBJECT_LOCK (overlay);

	if (G_UNLIKELY (overlay->update_composition))
	{
		maria_mark_update_composition (overlay);
		overlay->update_composition = FALSE;
	}

	GST_OBJECT_UNLOCK (overlay);

	if (overlay->comp != NULL)
	{
		gst_video_overlay_composition_blend (overlay->comp, frame);
	}

	return GST_FLOW_OK;
}
