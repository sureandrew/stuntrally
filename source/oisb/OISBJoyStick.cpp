/*
The zlib/libpng License

Copyright (c) 2009-2010 Martin Preisler

This software is provided 'as-is', without any express or implied warranty. In no event will
the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial 
applications, and to alter it and redistribute it freely, subject to the following
restrictions:

    1. The origin of this software must not be misrepresented; you must not claim that 
		you wrote the original software. If you use this software in a product, 
		an acknowledgment in the product documentation would be appreciated but is 
		not required.

    2. Altered source versions must be plainly marked as such, and must not be 
		misrepresented as being the original software.

    3. This notice may not be removed or altered from any source distribution.
*/

#include "../ogre/common/Defines.h"
#include "OISBJoyStick.h"
#include "OISBAnalogAxisState.h"
#include "OISBDigitalState.h"

#include <OISJoyStick.h>

#include <boost/lexical_cast.hpp>

#include <stdio.h>

// TODO: finish sliders, POV

static unsigned int joystick_count = 0;

namespace OISB
{
	JoyStick::JoyStick(OIS::JoyStick* joy):
		mJoyStick(joy)
	{
		mID = joystick_count; joystick_count++;
		// unique name for each JS
		mName = mJoyStick->vendor() + "_" + toStr(mID);

		int num_axis = mJoyStick->getNumberOfComponents(OIS::OIS_Axis);
		for(int a = 0; a<num_axis; a++)
		{
			axis.push_back(new AnalogAxisState(this, "Axis "+toStr(a)));
		}

		int num_buttons = mJoyStick->getNumberOfComponents(OIS::OIS_Button);
		for(int a = 0; a<num_buttons; a++)
		{
			buttons.push_back(new DigitalState(this, "Button "+toStr(a)));
		}

		int num_slider = mJoyStick->getNumberOfComponents(OIS::OIS_Slider);
		for(int a = 0; a<num_slider; a++)
		{
			axis.push_back(new AnalogAxisState(this, "Slider "+toStr(a)));
		}

		int num_pov = mJoyStick->getNumberOfComponents(OIS::OIS_POV);
		for(int a = 0; a<num_pov; a++)
		{
			// TODO: fix POV
			buttons.push_back(new DigitalState(this, "POV "+toStr(a)));
		}

		// now add the states
		for(std::vector<AnalogAxisState*>::iterator it = axis.begin(); it != axis.end(); ++it)
			addState(*it);
		for(std::vector<DigitalState*>::iterator it = buttons.begin(); it != buttons.end(); ++it)
			addState(*it);
	}
			
	JoyStick::~JoyStick()
	{
		for(std::vector<AnalogAxisState*>::iterator it = axis.begin(); it != axis.end(); ++it)
		{
			removeState(*it);
			delete(*it);
		}
		axis.clear();
		for(std::vector<DigitalState*>::iterator it = buttons.begin(); it != buttons.end(); ++it)
		{
			removeState(*it);
			delete(*it);
		}
		buttons.clear();
	}

	const String& JoyStick::getName() const
	{
		return mName;
	}

	void JoyStick::process(Real delta)
	{
		const OIS::JoyStickState& state = mJoyStick->getJoyStickState();
		for(unsigned int a = 0; a < state.mAxes.size(); a++)
		{
			if(state.mAxes[a].absOnly)
				axis[a]->_setAbsoluteValue(state.mAxes[a].abs / (float)OIS::JoyStick::MAX_AXIS);
			else
				axis[a]->_setRelativeValue(state.mAxes[a].rel / (float)OIS::JoyStick::MAX_AXIS);
		}
		for(unsigned int b = 0; b < state.mButtons.size(); b++)
		{
			buttons[b]->_setValue(state.mButtons[b]);
		}

	}

}
