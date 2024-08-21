/* 

Builds pileline with decodebin, plays audio/video

To compile:
gcc -Wall decodebin.c -o decodebin -pthread -I/usr/local/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/i386-linux-gnu/glib-2.0/include  -L/usr/local/lib -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0  

To Run:
./decodebin /home/asuri/my_gstcode/SampleVideo_720x480_1mb.mp4

*/


#include <gst/gst.h>

GstElement *pipeline, *video, *audio;

static gboolean my_bus_callback (GstBus     *bus, GstMessage *msg,  gpointer    data);

static void
cb_newpad (GstElement *decodebin,
       GstPad     *pad,
       gpointer    data)
{
  GstCaps *caps;
  GstStructure *str;
  GstPad *videopad, *audiopad;

  g_print("Entry in cb_newpad\n");

  /* check media type */
  caps = gst_pad_query_caps (pad, NULL);
  str = gst_caps_get_structure (caps, 0);
  if ( g_strrstr (gst_structure_get_name (str), "video") != NULL )  
  {

     videopad = gst_element_get_static_pad (video, "sink");
     if (GST_PAD_IS_LINKED (videopad) == FALSE ) 
     {
         gst_pad_link (pad, videopad);
     }

    gst_caps_unref (caps);
    gst_object_unref (videopad);
    g_print("2. Exit in cb_newpad\n");
    return;
  }
  else if ( g_strrstr (gst_structure_get_name (str), "audio") != NULL )
  {
     audiopad = gst_element_get_static_pad (audio, "sink");
     if (GST_PAD_IS_LINKED (audiopad) == FALSE )
     {
         gst_pad_link (pad, audiopad);
     }

    gst_caps_unref (caps);
    gst_object_unref (audiopad);
    g_print("3. Exit in cb_newpad\n");
    return;
     
  }

  gst_caps_unref (caps);
  g_print("Exit in cb_newpad\n");
}

gint
main (gint   argc,
      gchar *argv[])
{
  GMainLoop *loop;
  GstElement *src, *dec, *videoconv, *videosink, *audioconv, *audiosink;
  GstPad *videopad, *audiopad;
  GstBus *bus;

  /* init GStreamer */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* make sure we have input */
  if (argc != 2) {
    g_print ("Usage: %s <filename>\n", argv[0]);
    return -1;
  }

  /* setup */
  pipeline = gst_pipeline_new ("pipeline");

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, my_bus_callback, loop);
  gst_object_unref (bus);

  src = gst_element_factory_make ("filesrc", "source");
  g_object_set (G_OBJECT (src), "location", argv[1], NULL);
  dec = gst_element_factory_make ("decodebin", "decoder");
  g_signal_connect (dec, "pad-added", G_CALLBACK (cb_newpad), NULL);
  gst_bin_add_many (GST_BIN (pipeline), src, dec, NULL);
  gst_element_link (src, dec);

  /* create video output */
  video = gst_bin_new ("videobin");
  videoconv = gst_element_factory_make ("videoconvert", "vconv");
  videopad = gst_element_get_static_pad (videoconv, "sink");
  videosink = gst_element_factory_make ("ximagesink", "sink");
  gst_bin_add_many (GST_BIN (video), videoconv, videosink, NULL);
  gst_element_link (videoconv, videosink);
  gst_element_add_pad (video,
      gst_ghost_pad_new ("sink", videopad));
  gst_object_unref (videopad);
  gst_bin_add (GST_BIN (pipeline), video);

  /* create audio output */
  audio = gst_bin_new ("audiobin");
  audioconv = gst_element_factory_make ("audioconvert", "aconv");
  audiopad = gst_element_get_static_pad (audioconv, "sink");
  audiosink = gst_element_factory_make ("alsasink", "sink");
  gst_bin_add_many (GST_BIN (audio), audioconv, audiosink, NULL);
  gst_element_link (audioconv, audiosink);
  gst_element_add_pad (audio,
      gst_ghost_pad_new ("sink", audiopad));
  gst_object_unref (audiopad);
  gst_bin_add (GST_BIN (pipeline), audio);

  /* run */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);

  /* cleanup */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));

  return 0;
}

static gboolean my_bus_callback (GstBus     *bus, GstMessage *msg,  gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

