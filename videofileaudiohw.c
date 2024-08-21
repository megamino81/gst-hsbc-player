/*

[To build]
gcc -Wall videofileaudiohw.c -o videofileaudiohw -pthread -I/usr/local/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/i386-linux-gnu/glib-2.0/include  -L/usr/local/lib -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 

[To run]
./videofileaudiohw ./SampleVideo_720x480_1mb.mp4 ./out.h264

[Result]
Audio heard playing and video is written to out.h264 file. 

Played out.h264 file with::
gst-launch-1.0 filesrc location=./out.h264 ! h264parse ! avdec_h264 ! videoconvert ! ximagesink

*/


#include <gst/gst.h>

typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *source;
    GstElement *demuxer;
    GstElement *audioqueue;
    GstElement *videoqueue;
    GstElement *avc_parser;
    GstElement *audio_decoder;
    GstElement *video_decoder;
    GstElement *video_convert;
    GstElement *audio_convert;
    GstElement *video_sink;
    GstElement *audio_sink;
} CustomData;


static gboolean my_bus_callback (GstBus *bus, GstMessage *msg, gpointer data);
static void pad_added_handler (GstElement *src, GstPad *pad, CustomData *data);

int main(int argc, char *argv[]) 
{
    GstStructure *struc;
    GstCaps *caps;
    GMainLoop *loop;
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;
    /* Initialize GStreamer */
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    if (argc < 3)
    {
        g_print("\n Incorrect args to program");
        return 0;
    }


    /* Create the elements */
    data.source = gst_element_factory_make ("filesrc", "source");
    data.demuxer = gst_element_factory_make ("qtdemux", "demuxer");
    data.audioqueue = gst_element_factory_make("queue","audioqueue");
    data.audio_decoder = gst_element_factory_make ("faad", "audio_decoder");
    data.audio_convert = gst_element_factory_make ("audioconvert", "audio_convert");
    data.audio_sink = gst_element_factory_make ("autoaudiosink", "audio_sink");
    data.avc_parser = gst_element_factory_make ("h264parse", "avc_parser");
    data.videoqueue = gst_element_factory_make("queue","videoqueue");

    data.video_decoder = gst_element_factory_make("filesink","video_decoder");
    g_object_set (data.video_decoder, "location", argv[2], NULL);

    data.pipeline = gst_pipeline_new ("test-pipeline");

#if 1
     // Works for "nal"  but get an runtime warning 
    /* Specify what kind of video is wanted from the camera  nal or au are options */
    caps = gst_caps_new_simple( "video/x-h264",
     			        "stream-format", G_TYPE_STRING, "byte-stream",
                                NULL );
// 		                "alignment", G_TYPE_STRING, "nal",
//    			        NULL );
#else
     // Works for "AU" and not for NAL
     caps = gst_caps_new_empty();
     struc = gst_structure_new( "video/x-h264", 
                                "alignment", G_TYPE_STRING, "au", 
                                NULL );
     gst_caps_append_structure(caps, struc);
#endif 


    bus = gst_pipeline_get_bus (GST_PIPELINE (data.pipeline));
    gst_bus_add_watch (bus, my_bus_callback, loop);
    gst_object_unref (bus);

    if (!data.pipeline || !data.source || !data.demuxer || !data.audioqueue ||!data.audio_decoder ||!data.audio_convert ||
    !data.audio_sink || !data.videoqueue || !data.video_decoder || !data.avc_parser) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
    }

    gst_bin_add_many (GST_BIN (data.pipeline), data.source,data.demuxer,data.audioqueue,data.audio_decoder,data.audio_convert,data.audio_sink,data.videoqueue,data.avc_parser, data.video_decoder, NULL);

    if (!gst_element_link(data.source,data.demuxer)) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (data.pipeline);
    return -1;
    } 

    if (!gst_element_link_many (data.audioqueue,data.audio_decoder,data.audio_convert, data.audio_sink,NULL)) {
    g_printerr (" audio Elements could not be linked.\n");
    gst_object_unref (data.pipeline);
    return -1;
    }
  
    if (!gst_element_link_many(data.videoqueue, data.avc_parser, NULL)) {
          g_printerr("video Elements could not be linked.\n");
         gst_object_unref(data.pipeline);
        return -1;
    }
   
    if ( ! gst_element_link_filtered(data.avc_parser, data.video_decoder, caps) )
    {
       g_printerr (" Failed to link with filetered.\n");
       gst_object_unref (data.pipeline);
       gst_caps_unref(caps);
       return -1;
 
    }

#if 0
    if (!gst_element_link_many( data.video_decoder, data.video_convert, data.video_sink,NULL)) {
    g_printerr("video Elements could not be linked.\n");
    gst_object_unref(data.pipeline);
    return -1;
    } 
#endif 
    g_print("Reached here\n");
    /* Set the file to play */
    g_object_set (data.source, "location", argv[1], NULL);

    g_signal_connect (data.demuxer, "pad-added", G_CALLBACK (pad_added_handler), &data);
    /* Start playing */
    ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
    g_main_loop_run (loop);

    if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (data.pipeline);
    return -1;
    }
    gst_object_unref (bus);
    gst_caps_unref(caps);
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
   
#if 0 
    
    /* Listen to the bus */
    bus = gst_element_get_bus (data.pipeline);
    do {
    msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
    GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    if (msg != NULL) {
    GError *err;
    gchar *debug_info;
    switch (GST_MESSAGE_TYPE (msg)) 
    {
    case GST_MESSAGE_ERROR:
    gst_message_parse_error (msg, &err, &debug_info);
    g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
    g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error (&err);
    g_free (debug_info);
    terminate = TRUE;
    break;
    case GST_MESSAGE_EOS:
    g_print ("End-Of-Stream reached.\n");
    terminate = TRUE;
    break;
    case GST_MESSAGE_STATE_CHANGED:

    if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data.pipeline)) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
    g_print ("Pipeline state changed from %s to %s:\n",
    gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
    }
    break;
    default:

    g_printerr ("Unexpected message received.\n");
    break;
    }
    gst_message_unref (msg);
    }
    } while (!terminate); 

    gst_object_unref (bus);
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
#endif 
    return 0;
    }

/* This function will be called by the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *new_pad, CustomData *data) 
{
    GstPad *sink_pad_audio = gst_element_get_static_pad (data->audioqueue, "sink");
    
    GstPad *sink_pad_video = gst_element_get_static_pad (data->videoqueue, "sink");

    GstCaps *new_pad_caps;
    GstStructure *str;
    GstPad *videopad, *audiopad;
    const gchar *new_pad_type;
    GstPadLinkReturn ret;

    g_print("Entry in cb_newpad\n");
    g_print("Inside the pad_added_handler method \n");

    new_pad_caps = gst_pad_query_caps (new_pad, NULL);
    str = gst_caps_get_structure (new_pad_caps, 0);
    new_pad_type = gst_structure_get_name (str);

    if ( g_strrstr (gst_structure_get_name (str), "audio") != NULL )
    {
      ret = gst_pad_link (new_pad, sink_pad_audio);
      if (GST_PAD_LINK_FAILED (ret)) 
       { 
        g_print (" Type is '%s' but link failed.\n", new_pad_type);
       } 
       else 
      {
        g_print (" Link succeeded (type '%s').\n", new_pad_type);
      }
    } 
    else if ( g_strrstr (gst_structure_get_name (str), "video") != NULL )
    {
      ret = gst_pad_link (new_pad, sink_pad_video);

      if (GST_PAD_LINK_FAILED (ret)) 
      {
        g_print (" Type is '%s' but link failed.\n", new_pad_type);
      } 
      else 
      {
        g_print (" Link succeeded (type '%s').\n", new_pad_type);
      }
    } 


    else {
    g_print (" It has type '%s' which is not raw audio. Ignoring.\n", new_pad_type);
    goto exit;
    }
    exit:
    if (new_pad_caps != NULL)
    gst_caps_unref (new_pad_caps);
    gst_object_unref (sink_pad_audio);
    gst_object_unref (sink_pad_video);
}

static gboolean my_bus_callback (GstBus *bus, GstMessage *msg, gpointer data)
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

