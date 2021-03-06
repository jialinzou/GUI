/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2014 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "SourceNode.h"
#include "../DataThreads/DataBuffer.h"
//#include "DataThreads/IntanThread.h"
#include "../DataThreads/FPGAThread.h"
//#include "../DataThreads/FileReaderThread.h"
#include "../DataThreads/RHD2000Thread.h"
#include "../DataThreads/EcubeThread.h" // Added by Michael Borisov
#include "../SourceNode/SourceNodeEditor.h"
//#include "../FileReader/FileReaderEditor.h"
#include "../DataThreads/RHD2000Editor.h"
#include "../DataThreads/EcubeEditor.h" // Added by Michael Borisov
#include "../Channel/Channel.h"
#include <stdio.h>

SourceNode::SourceNode(const String& name_)
    : GenericProcessor(name_),
      sourceCheckInterval(2000), wasDisabled(true), dataThread(0),
      inputBuffer(0), ttlState(0)
{

    std::cout << "creating source node." << std::endl;

    if (getName().equalsIgnoreCase("RHA2000-EVAL"))
    {
        // dataThread = new IntanThread(this);o
    }
    else if (getName().equalsIgnoreCase("Custom FPGA"))
    {
        dataThread = new FPGAThread(this);
    }
    //else if (getName().equalsIgnoreCase("File Reader"))
   // {
    //    dataThread = new FileReaderThread(this);
   // }
    else if (getName().equalsIgnoreCase("Rhythm FPGA"))
    {
        dataThread = new RHD2000Thread(this);
    }
#if ECUBE_COMPILE
    else if (getName().equalsIgnoreCase("eCube"))
    {
        dataThread = new EcubeThread(this);
    }
#endif

    if (dataThread != 0)
    {
        if (!dataThread->foundInputSource())
        {
            enabledState(false);
        }

        numEventChannels = dataThread->getNumEventChannels();
        eventChannelState = new int[numEventChannels];
        for (int i = 0; i < numEventChannels; i++)
        {
            eventChannelState[i] = 0;
        }

    }
    else
    {
        enabledState(false);
        eventChannelState = 0;
        numEventChannels = 0;
    }

    // check for input source every few seconds
    startTimer(sourceCheckInterval);

    timestamp = 0;
    eventCodeBuffer = new uint64[10000]; //10000 samples per buffer max?


}

SourceNode::~SourceNode()
{

    if (dataThread->isThreadRunning())
    {
        std::cout << "Forcing thread to stop." << std::endl;
        dataThread->stopThread(500);
    }


    if (eventChannelState)
        delete[] eventChannelState;
}

DataThread* SourceNode::getThread()
{
    return dataThread;
}

int SourceNode::modifyChannelName(channelType t, int str, int ch, String newName, bool updateSignalChain)
{
    if (dataThread != 0) {
        int channel_index = dataThread->modifyChannelName(t, str, ch, newName);
        if (channel_index >= 0 && channel_index < channels.size())
        {
            if (channels[channel_index]->getChannelName() != newName)
            {
                channels[channel_index]->setName(newName);
                // propagate this information...
                
                if (updateSignalChain)
                    getEditorViewport()->makeEditorVisible(getEditor(), false, true);
                    
            }
        }
        return channel_index;
    }
    return -1;
}

int SourceNode::modifyChannelGain(int stream, int channel,channelType type, float gain, bool updateSignalChain)
{
    if (dataThread != 0) 
    {
        
        int channel_index = dataThread->modifyChannelGain(type, stream, channel, gain);
        
        if (channel_index >= 0 && channel_index < channels.size())
        {
            // we now need to update the signal chain to propagate this change.....
            if (channels[channel_index]->getChannelGain() != gain) 
            {
                channels[channel_index]->setGain(gain);
                
                if (updateSignalChain)
                    getEditorViewport()->makeEditorVisible(getEditor(), false, true);
                
                return channel_index;
            }
        }
    }

    return -1;
}

void SourceNode::getChannelsInfo(StringArray &names, Array<channelType> &types, Array<int> &stream, Array<int> &originalChannelNumber, Array<float> &gains)
{
    if (dataThread != 0)
        dataThread->getChannelsInfo(names, types,stream,originalChannelNumber,gains);
}

void SourceNode::setDefaultNamingScheme(int scheme)
{
    if (dataThread != 0) 
    {
        dataThread->setDefaultNamingScheme(scheme);

        StringArray names;
        Array<channelType> types;
        Array<int> stream;
        Array<int> originalChannelNumber;
        Array<float> gains;
        getChannelsInfo(names, types, stream, originalChannelNumber, gains);
        for (int k = 0; k < names.size(); k++)
        {
            modifyChannelName(types[k],stream[k],originalChannelNumber[k], names[k],false);
        }
    }

}

void SourceNode::getEventChannelNames(StringArray &names)
{
    if (dataThread != 0)
        dataThread->getEventChannelNames(names);

}

void SourceNode::updateSettings()
{
    if (inputBuffer == 0 && dataThread != 0)
    {

        inputBuffer = dataThread->getBufferAddress();
        std::cout << "Input buffer address is " << inputBuffer << std::endl;
    }

	dataThread->updateChannelNames();

    for (int i = 0; i < dataThread->getNumEventChannels(); i++)
    {
        Channel* ch = new Channel(this, i);
        ch->eventType = TTL;
        ch->getType() == EVENT_CHANNEL;
        eventChannels.add(ch);
    }

   //for (int i = 0; i < channels.size(); i++)
   // {
        std::cout << "Channel: " << channels[channels.size()-1]->bitVolts << std::endl;
    //}


}

void SourceNode::actionListenerCallback(const String& msg)
{

    //std::cout << msg << std::endl;

    if (msg.equalsIgnoreCase("HI"))
    {
        // std::cout << "HI." << std::endl;
        // dataThread->setOutputHigh();
        ttlState = 1;
    }
    else if (msg.equalsIgnoreCase("LO"))
    {
        // std::cout << "LO." << std::endl;
        // dataThread->setOutputLow();
        ttlState = 0;
    }
}

int SourceNode::getTTLState()
{
    return ttlState;
}

float SourceNode::getSampleRate()
{

    if (dataThread != 0)
        return dataThread->getSampleRate();
    else
        return 44100.0;
}

float SourceNode::getDefaultSampleRate()
{
    if (dataThread != 0)
        return dataThread->getSampleRate();
    else
        return 44100.0;
}

int SourceNode::getDefaultNumOutputs()
{
    if (dataThread != 0)
        return dataThread->getNumChannels();
    else
        return 0;
}

float SourceNode::getBitVolts(int chan)
{
	if (dataThread != 0)
		return dataThread->getBitVolts(chan);
	else
		return 1.0f;
}


void SourceNode::enabledState(bool t)
{
    if (t && !dataThread->foundInputSource())
    {
        isEnabled = false;
    }
    else
    {
        isEnabled = t;
    }

}

void SourceNode::setParameter(int parameterIndex, float newValue)
{
    editor->updateParameterButtons(parameterIndex);
    //std::cout << "Got parameter change notification";
}

AudioProcessorEditor* SourceNode::createEditor()
{

    if (getName().equalsIgnoreCase("Rhythm FPGA"))
    {
        editor = new RHD2000Editor(this, (RHD2000Thread*) dataThread.get(), true);

        //  RHD2000Editor* r2e = (RHD2000Editor*) editor.get();
        //  r2e->scanPorts();
    }
    //  else if (getName().equalsIgnoreCase("File Reader"))
    //  {
    //     editor = new FileReaderEditor(this, (FileReaderThread*) dataThread.get(), true);
    // }
    else
    {
        editor = new SourceNodeEditor(this, true);
    }
    return editor;
}

bool SourceNode::tryEnablingEditor()
{
    if (!isReady())
    {
        //std::cout << "No input source found." << std::endl;
        return false;
    }
    else if (isEnabled)
    {
        // If we're already enabled (e.g. if we're being called again
        // due to timerCallback()), then there's no need to go through
        // the editor again.
        return true;
    }

    std::cout << "Input source found." << std::endl;
    enabledState(true);
    GenericEditor* ed = getEditor();
    getEditorViewport()->makeEditorVisible(ed);
    return true;
}

void SourceNode::timerCallback()
{
    if (!tryEnablingEditor() && isEnabled)
    {
        std::cout << "Input source lost." << std::endl;
        enabledState(false);
        GenericEditor* ed = getEditor();
        getEditorViewport()->makeEditorVisible(ed);
    }
}

bool SourceNode::isReady()
{
    return dataThread && dataThread->foundInputSource();
}

bool SourceNode::enable()
{

    std::cout << "Source node received enable signal" << std::endl;

    wasDisabled = false;

    if (dataThread != 0)
    {
        dataThread->startAcquisition();
        return true;
    }
    else
    {
        return false;
    }

    stopTimer(); // WARN compiler warning: unreachable code (spotted by Michael Borisov). Probably needs to be removed

}

bool SourceNode::disable()
{

    std::cout << "Source node received disable signal" << std::endl;

    if (dataThread != 0)
        dataThread->stopAcquisition();

    startTimer(2000);

    wasDisabled = true;

    std::cout << "SourceNode returning true." << std::endl;

    return true;
}

void SourceNode::acquisitionStopped()
{
    //if (!dataThread->foundInputSource()) {

    if (!wasDisabled)
    {
        std::cout << "Source node sending signal to UI." << std::endl;
        getUIComponent()->disableCallbacks();
        enabledState(false);
        GenericEditor* ed = (GenericEditor*) getEditor();
        getEditorViewport()->makeEditorVisible(ed);
    }
    //}
}


void SourceNode::process(AudioSampleBuffer& buffer,
                         MidiBuffer& events,
                         int& nSamples)
{

    //std::cout << "SOURCE NODE" << std::endl;

    // clear the input buffers
    events.clear();
    buffer.clear();

    nSamples = inputBuffer->readAllFromBuffer(buffer, &timestamp, eventCodeBuffer, buffer.getNumSamples());

    //std::cout << *buffer.getReadPointer(0) << std::endl;

    //std::cout << "Source node timestamp: " << timestamp << std::endl;

    //std::cout << "Samples per buffer: " << nSamples << std::endl;

    uint8 data[8];
    memcpy(data, &timestamp, 8);

    // generate timestamp
    addEvent(events,    // MidiBuffer
             TIMESTAMP, // eventType
             0,         // sampleNum
             nodeId,    // eventID
             0,		 // eventChannel
             8,         // numBytes
             data   // data
            );

    // std::cout << (int) *(data + 7) << " " <<
    //                 (int) *(data + 6) << " " <<
    //                 (int) *(data + 5) << " " <<
    //                 (int) *(data + 4) << " " <<
    //                 (int) *(data + 3) << " " <<
    //                 (int) *(data + 2) << " " <<
    //                 (int) *(data + 1) << " " <<
    //                 (int) *(data + 0) << std::endl;


    // fill event buffer
    for (int i = 0; i < nSamples; i++)
    {
        for (int c = 0; c < numEventChannels; c++)
        {
            int state = eventCodeBuffer[i] & (1 << c);

            if (eventChannelState[c] != state)
            {
                if (state == 0)
                {

                    //std::cout << "OFF" << std::endl;
                    //std::cout << c << std::endl;
                    // signal channel state is OFF
                    addEvent(events, // MidiBuffer
                             TTL,    // eventType
                             i,      // sampleNum
                             0,	     // eventID
                             c		 // eventChannel
                            );
                }
                else
                {

                    // std::cout << "ON" << std::endl;
                    // std::cout << c << std::endl;

                    // signal channel state is ON
                    addEvent(events, // MidiBuffer
                             TTL,    // eventType
                             i,      // sampleNum
                             1,		 // eventID
                             c		 // eventChannel
                            );


                }

                eventChannelState[c] = state;
            }
        }
    }

}



void SourceNode::saveCustomParametersToXml(XmlElement* parentElement)
{

    StringArray names;
    Array<channelType> types;
    Array<int> stream;
    Array<int> originalChannelNumber;
    Array<float> gains;
    getChannelsInfo(names, types, stream, originalChannelNumber, gains);
	XmlElement *channelInfo = parentElement->createNewChildElement("CHANNEL_INFO");
	for (int i = 0; i < names.size(); i++)
	{
		XmlElement* chan = channelInfo->createNewChildElement("CHANNEL");
		chan->setAttribute("name",names[i]);
		chan->setAttribute("stream",stream[i]);
		chan->setAttribute("number",originalChannelNumber[i]);
		chan->setAttribute("type",(int)types[i]);
		chan->setAttribute("gain",gains[i]);
	}

}

void SourceNode::loadCustomParametersFromXml()
{

    if (parametersAsXml != nullptr)
    {
        // use parametersAsXml to restore state

        forEachXmlChildElement(*parametersAsXml, xmlNode)
        {
           if (xmlNode->hasTagName("CHANNEL_INFO"))
            {
				forEachXmlChildElementWithTagName(*xmlNode,chan,"CHANNEL")
				{
					String name = chan->getStringAttribute("name");
					int stream = chan->getIntAttribute("stream");
					int number = chan->getIntAttribute("number");
					channelType type = static_cast<channelType>(chan->getIntAttribute("type"));
					float gain = chan->getDoubleAttribute("gain");
					modifyChannelName(type,stream,number,name,false);
					modifyChannelGain(stream,number,type,gain,false);					
				}
            }
        }
    }

}