/*
    Copyright (C) 2010 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
#include <stdlib.h>
#include "tracks_control_panel.h"
#include "waves_button.h"
//#include <vector>
//#include <cmath>
//#include <fstream>
//#include <map>

//#include <boost/scoped_ptr.hpp>

//#include <gtkmm/messagedialog.h>

//#include "pbd/error.h"
//#include "pbd/xml++.h"
#include "pbd/unwind.h"
//#include "pbd/failed_constructor.h"

//#include <gtkmm/alignment.h>
//#include <gtkmm/stock.h>
//#include <gtkmm/notebook.h>
#include <gtkmm2ext/utils.h>

#include "ardour/audio_backend.h"
#include "ardour/audioengine.h"
//#include "ardour/mtdm.h"
//#include "ardour/rc_configuration.h"
//#include "ardour/types.h"

//#include "pbd/convert.h"
//#include "pbd/error.h"

#include "ardour_ui.h"
//#include "tracks_control_panel.h"
#include "gui_thread.h"
#include "utils.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;

#define dbg_msg(a) MessageDialog (a, PROGRAM_NAME).run();

void
TracksControlPanel::init ()
{
	ok_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_ok));
	cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_cancel));
	apply_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_apply));

	audio_settings_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_audio_settings));
	midi_settings_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_midi_settings));
	control_panel_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_control_panel));

	ARDOUR::AudioEngine::instance ()->Running.connect (running_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_running, this), gui_context());
	ARDOUR::AudioEngine::instance ()->Stopped.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_stopped, this), gui_context());
	ARDOUR::AudioEngine::instance ()->Halted.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_stopped, this), gui_context());

	/* Subscribe for udpates from AudioEngine */
	ARDOUR::AudioEngine::instance()->BufferSizeChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::update_current_buffer_size, this, _1), gui_context());
    ARDOUR::AudioEngine::instance()->DeviceListChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::update_device_list, this), gui_context());

	engine_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::engine_changed));
	device_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::device_changed));
	sample_rate_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::sample_rate_changed));
	buffer_size_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::buffer_size_changed));

	populate_engine_combo ();
	show_all ();
	midi_settings_layout.hide ();
	audio_settings_tab_button.set_active(true);
}

void
TracksControlPanel::populate_engine_combo()
{
	if (_ignore_changes) {
		return;
	}

	vector<string> strings;
	vector<const ARDOUR::AudioBackendInfo*> backends = ARDOUR::AudioEngine::instance()->available_backends();

	if (backends.empty()) {
		MessageDialog msg (string_compose (_("No audio/MIDI backends detected. %1 cannot run\n\n(This is a build/packaging/system error. It should never happen.)"), PROGRAM_NAME));
		msg.run ();
		throw failed_constructor ();
	}
	for (vector<const ARDOUR::AudioBackendInfo*>::const_iterator b = backends.begin(); b != backends.end(); ++b) {
		strings.push_back ((*b)->name);
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (engine_combo, strings);
		engine_combo.set_sensitive (strings.size() > 1);
	}

	if (!strings.empty() )
	{
		engine_combo.set_active_text (strings.front());
	}
}

void
TracksControlPanel::populate_device_combo()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	vector<ARDOUR::AudioBackend::DeviceStatus> all_devices = backend->enumerate_devices ();
	vector<string> available_devices;

	for (vector<ARDOUR::AudioBackend::DeviceStatus>::const_iterator i = all_devices.begin(); i != all_devices.end(); ++i) {
		available_devices.push_back (i->name);
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (device_combo, available_devices);
		device_combo.set_sensitive (available_devices.size() > 1);
	}

	if(!available_devices.empty() ) {
		device_combo.set_active_text (available_devices.front() );
	}

}

void
TracksControlPanel::populate_sample_rate_combo()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);
	std::string device_name = device_combo.get_active_text ();
	std::vector<std::string> s;
	vector<float> sr;

	sr = backend->available_sample_rates (device_name);
	for (vector<float>::const_iterator x = sr.begin(); x != sr.end(); ++x) {
		s.push_back (rate_as_string (*x));
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (sample_rate_combo, s);
		sample_rate_combo.set_sensitive (s.size() > 1);
	}

	if (!s.empty() ) {
		std::string active_sr = rate_as_string((_desired_sample_rate != 0) ? 
													_desired_sample_rate :
													backend->default_sample_rate());
		if (std::find(s.begin(), s.end(), active_sr) == s.end()) {
			active_sr = s.front();
		} 
		sample_rate_combo.set_active_text(active_sr);
	}
}

void
TracksControlPanel::populate_buffer_size_combo()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);
	std::string device_name = device_combo.get_active_text ();
	std::vector<std::string> s;
	std::vector<uint32_t> bs;

	bs = backend->available_buffer_sizes(device_name);
	for (std::vector<uint32_t>::const_iterator x = bs.begin(); x != bs.end(); ++x) {
		s.push_back (bufsize_as_string (*x));
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (buffer_size_combo, s);
		buffer_size_combo.set_sensitive (s.size() > 1);
	}

	if (!s.empty() ) {
		std::string active_bs = bufsize_as_string(backend->default_buffer_size());
		if (std::find(s.begin(), s.end(), active_bs) == s.end() ) {
			active_bs = s.front();
		} 
		buffer_size_combo.set_active_text(active_bs);
	}
}

void
TracksControlPanel::on_control_panel(WavesButton*)
{
}

void TracksControlPanel::engine_changed ()
{
	if (_ignore_changes) {
		return;
	}

	string backend_name = engine_combo.get_active_text();
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->set_backend (backend_name, "ardour", "");
	if (!backend)
	{
		std::cerr << "\tfailed to set backend [" << backend_name << "]\n";
		return;
	}

	_have_control = ARDOUR::AudioEngine::instance()->setup_required ();

	populate_device_combo();
}

void TracksControlPanel::device_changed ()
{
	if (_ignore_changes) {
		return;
	}

    string newDevice = device_combo.get_active_text();
    if (newDevice != "None") {
        _current_device = newDevice;
    }
    
	populate_buffer_size_combo();
	populate_sample_rate_combo();
}

void 
TracksControlPanel::buffer_size_changed()
{
	if (_ignore_changes) {
		return;
	}

	show_buffer_duration();
}

void
TracksControlPanel::sample_rate_changed()
{
	if (_ignore_changes) {
		return;
	}

	show_buffer_duration();
}

void
TracksControlPanel::update_current_buffer_size (uint32_t new_buffer_size)
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);
    
    string new_buffer_size_str = bufsize_as_string(new_buffer_size);
    
    /* check if new buffer size value is no the same as already set in combobox */
    if ( new_buffer_size_str != buffer_size_combo.get_active_text() ) {
        vector<string> s;
        vector<uint32_t> bs;
        string device_name = device_combo.get_active_text ();
        bs = backend->available_buffer_sizes(device_name);
        
        for (vector<uint32_t>::const_iterator x = bs.begin(); x != bs.end(); ++x) {
            s.push_back (bufsize_as_string (*x) );
        }
        
		{
			// set _ignore_changes flag to ignore changes in combo-box callbacks
			PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
			set_popdown_strings (buffer_size_combo, s);
			buffer_size_combo.set_sensitive (s.size() > 1);
		}

        if (!s.empty() ) {
            if (std::find(s.begin(), s.end(), new_buffer_size_str) == s.end() ) {
				buffer_size_combo.set_active_text (new_buffer_size_str );
			} else {
				MessageDialog( _("Buffer size changed to the value which is not supported"), PROGRAM_NAME).run();
			}
        }
    }
}


void
TracksControlPanel::update_device_list ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);
    
	/* now fill out devices, mark sample rates, buffer sizes insensitive */
    
	vector<ARDOUR::AudioBackend::DeviceStatus> all_devices = backend->enumerate_devices ();
    
	vector<string> available_devices;
    
	for (vector<ARDOUR::AudioBackend::DeviceStatus>::const_iterator i = all_devices.begin(); i != all_devices.end(); ++i) {
		available_devices.push_back (i->name);
	}
    
	if (!available_devices.empty()) {
            
		/* Now get current device name */
		string current_active_device = device_combo.get_active_text ();

        /* If previous device is available again we should switch to it from "None" */
        string newDevice;
        if (current_active_device == "None" && !_current_device.empty() ){
            newDevice = _current_device;
        } else {
            newDevice = current_active_device;
        }
        
		bool deviceFound(false);
		{
			// set _ignore_changes flag to ignore changes in combo-box callbacks
			PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
			set_popdown_strings (device_combo, available_devices);
			device_combo.set_sensitive (available_devices.size() > 1);

			for (vector<string>::const_iterator i = available_devices.begin(); i != available_devices.end(); ++i) {
				if (newDevice == *i) {
					deviceFound = true;
					break;
				}
			}
		}
        
        if (deviceFound) {
            switch_to_device(newDevice);
            
        } else if (current_active_device != "None") {
			switch_to_device("None");
            MessageDialog( _("Current device is not available! Switched to NONE device."), PROGRAM_NAME).run();
		}
	}
}


void
TracksControlPanel::switch_to_device(const string& device_name)
{
    boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
    
    {
        // set _ignore_changes flag to ignore changes in combo-box callbacks
        PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
        device_combo.set_active_text(device_name);
    }
    
    if (backend->device_name() != device_name) {

        // push state to backend, but do not start unless it was running
        push_state_to_backend (false);
    }
}


void
TracksControlPanel::engine_running ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	buffer_size_combo.set_active_text (bufsize_as_string (backend->buffer_size()));

	sample_rate_combo.set_active_text (rate_as_string (backend->sample_rate()));

	buffer_size_combo.set_sensitive (true);
	sample_rate_combo.set_sensitive (true);
}


void
TracksControlPanel::engine_stopped ()
{
}


void
TracksControlPanel::on_audio_settings (WavesButton*)
{
	midi_settings_layout.hide ();
	midi_settings_tab_button.set_active(false);
	audio_settings_layout.show ();
	audio_settings_tab_button.set_active(true);
}


void
TracksControlPanel::on_midi_settings (WavesButton*)
{
	audio_settings_layout.hide ();
	audio_settings_tab_button.set_active(false);
	midi_settings_layout.show ();
	midi_settings_tab_button.set_active(true);
}


void
TracksControlPanel::on_ok (WavesButton*)
{
	hide();
	push_state_to_backend (true);
	response(Gtk::RESPONSE_OK);
}


void
TracksControlPanel::on_cancel (WavesButton*)
{
	hide();
	response(Gtk::RESPONSE_CANCEL);
}


void 
TracksControlPanel::on_apply (WavesButton*)
{
	//push_state_to_backend (true);
	response(Gtk::RESPONSE_APPLY);
}

std::string
TracksControlPanel::bufsize_as_string (uint32_t sz)
{
	/* Translators: "samples" is always plural here, so no
	need for plural+singular forms.
	*/
	char  buf[32];
	snprintf (buf, sizeof (buf), _("%u samples"), sz);
	return buf;
}

void
TracksControlPanel::set_desired_sample_rate (uint32_t sr)
 {
	_desired_sample_rate = sr;
	std::string active_sr = rate_as_string(_desired_sample_rate);
	sample_rate_combo.set_active_text(active_sr);
 }

XMLNode&
TracksControlPanel::get_state ()
{
	return *new XMLNode ("TracksPreferences");// !!!!!!!!!!!!!!!
	/*
	XMLNode* root = new XMLNode ("TracksPreferences");
	std::string path;

	if (!states.empty()) {
		XMLNode* state_nodes = new XMLNode ("EngineStates");

		for (StateList::const_iterator i = states.begin(); i != states.end(); ++i) {

			XMLNode* node = new XMLNode ("State");

			node->add_property ("backend", (*i).backend);
			node->add_property ("driver", (*i).driver);
			node->add_property ("device", (*i).device);
			node->add_property ("sample-rate", (*i).sample_rate);
			node->add_property ("buffer-size", (*i).buffer_size);
			node->add_property ("input-latency", (*i).input_latency);
			node->add_property ("output-latency", (*i).output_latency);
			node->add_property ("input-channels", (*i).input_channels);
			node->add_property ("output-channels", (*i).output_channels);
			node->add_property ("active", (*i).active ? "yes" : "no");
			node->add_property ("midi-option", (*i).midi_option);

			state_nodes->add_child_nocopy (*node);
		}

		root->add_child_nocopy (*state_nodes);
	}

	return *root;
	*/
}

void 
TracksControlPanel::set_state (const XMLNode& root)
{
}

float
TracksControlPanel::get_rate () const
{
	float r = atof (sample_rate_combo.get_active_text ());
	/* the string may have been translated with an abbreviation for
	* thousands, so use a crude heuristic to fix this.
	*/
	if (r < 1000.0) {
		r *= 1000.0;
	}
	return r;
}

uint32_t TracksControlPanel::get_buffer_size() const
{
	string bs_text = buffer_size_combo.get_active_text ();
	uint32_t samples = atoi (bs_text); /* will ignore trailing text */
	return samples;
}

void
TracksControlPanel::show_buffer_duration ()
{
	 float latency = (get_buffer_size() * 1000.0) / get_rate();

	 char buf[256];
	 snprintf (buf, sizeof (buf), _("INPUT LATENCY: %.1f MS      OUTPUT LATENCY: %.1f MS      TOTAL LATENCY: %.1f MS"), 
			   latency, latency, 2*latency);
	 latency_label.set_text (buf);
}

int
TracksControlPanel::push_state_to_backend (bool start)
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	
	if (!backend) {
		return 0;
	}

	bool was_running = ARDOUR::AudioEngine::instance()->running();
	if (was_running) {
		if (ARDOUR_UI::instance()->disconnect_from_engine ()) {
				 return -1;
			 }
	}

	if ((get_device_name() != backend->device_name()) && backend->set_device_name (get_device_name())) {
		error << string_compose (_("Cannot set device name to %1"), get_device_name()) << endmsg;
		return -1;
	}

	if (backend->set_sample_rate (get_rate())) {
		error << string_compose (_("Cannot set sample rate to %1"), get_rate()) << endmsg;
		return -1;
	}

	if (backend->set_buffer_size (get_buffer_size())) {
		error << string_compose (_("Cannot set buffer size to %1"), get_buffer_size()) << endmsg;
		return -1;
	}

	//if (backend->set_input_channels (get_input_channels())) {
	//	error << string_compose (_("Cannot set input channels to %1"), get_input_channels()) << endmsg;
	//	return -1;
	//}

	//if (backend->set_output_channels (get_output_channels())) {
	//	error << string_compose (_("Cannot set output channels to %1"), get_output_channels()) << endmsg;
	//	return -1;
	//}

	//if (backend->set_systemic_input_latency (get_input_latency())) {
	//	error << string_compose (_("Cannot set input latency to %1"), get_input_latency()) << endmsg;
	//	return -1;
	//}

	//if (backend->set_systemic_output_latency (get_output_latency())) {
	//	error << string_compose (_("Cannot set output latency to %1"), get_output_latency()) << endmsg;
	//	return -1;
	//}

	if(start || was_running)
	{
		//dbg_msg("ARDOUR_UI::instance()->reconnect_to_engine ()");
		ARDOUR_UI::instance()->reconnect_to_engine ();
		//dbg_msg("Done");
	}
	return 0;
}
