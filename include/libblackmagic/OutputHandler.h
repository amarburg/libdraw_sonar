#pragma once

#include <DeckLinkAPI.h>
#include <active_object/active.h>

#include "SDICameraControl.h"

namespace libblackmagic {

	class SharedBMSDIBuffer {
	public:

		typedef std::lock_guard<std::mutex> lock_guard;

		SharedBMSDIBuffer()
			: buffer( bmNewBuffer() ), _writeMutex()
		{;}

		std::mutex &writeMutex()
			{ return _writeMutex; }

		BMSDIBuffer *buffer;
		std::mutex _writeMutex;
	};




	class OutputHandler: public IDeckLinkVideoOutputCallback
	{
	public:
		OutputHandler( IDeckLinkOutput *deckLinkOutput,
									 IDeckLinkDisplayMode *mode );
		virtual ~OutputHandler(void);

		//void setBMSDIBuffer( const std::shared_ptr<SharedBMBuffer> &buffer );

		const std::shared_ptr<SharedBMSDIBuffer> &sdiProtocolBuffer();

		HRESULT	STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result);

		HRESULT	STDMETHODCALLTYPE ScheduledPlaybackHasStopped(void) {	return S_OK; }

		// Dummy implementations
		HRESULT	STDMETHODCALLTYPE QueryInterface (REFIID iid, LPVOID *ppv){ return E_NOINTERFACE; }
		ULONG STDMETHODCALLTYPE AddRef() { return 1; }
		ULONG STDMETHODCALLTYPE Release() { return 1; }

	protected:

		void scheduleFrame( IDeckLinkVideoFrame *frame, uint8_t count = 1 );

	private:

		IDeckLinkOutput *_deckLinkOutput;
		IDeckLinkDisplayMode *_mode;

		// Cached values
		BMDTimeValue _timeValue;
		BMDTimeScale _timeScale;

		//BMSDIBuffer *_bmsdiBuffer;

		unsigned int _totalFramesScheduled;

		std::shared_ptr<SharedBMSDIBuffer> _buffer;
		IDeckLinkMutableVideoFrame *_blankFrame;

	};

}
