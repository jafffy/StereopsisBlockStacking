#include "pch.h"
#include "SpatialInputHandler.h"
#include <functional>

using namespace StereopsisBlockStacking;

using namespace Windows::Foundation;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;

// Creates and initializes a GestureRecognizer that listens to a Person.
SpatialInputHandler::SpatialInputHandler()
{
	// The interaction manager provides an event that informs the app when
	// spatial interactions are detected.
	m_interactionManager = SpatialInteractionManager::GetForCurrentView();

	// Bind a handler to the SourcePressed event.
	m_sourcePressedEventToken =
		m_interactionManager->SourcePressed +=
		ref new TypedEventHandler<SpatialInteractionManager^, SpatialInteractionSourceEventArgs^>(
			bind(&SpatialInputHandler::OnSourcePressed, this, _1, _2)
			);
	m_sourceReleasedEventToken =
		m_interactionManager->SourceReleased +=
		ref new TypedEventHandler<SpatialInteractionManager^, SpatialInteractionSourceEventArgs^>(
			bind(&SpatialInputHandler::OnSourceReleased, this, _1, _2)
			);
	m_sourceUpdatedEventToken =
		m_interactionManager->SourceUpdated +=
		ref new TypedEventHandler<SpatialInteractionManager^, SpatialInteractionSourceEventArgs^>(
			bind(&SpatialInputHandler::OnSourceUpdated, this, _1, _2)
			);
}

SpatialInputHandler::~SpatialInputHandler()
{
	m_interactionManager->SourcePressed -= m_sourcePressedEventToken;
	m_interactionManager->SourceReleased -= m_sourceReleasedEventToken;
	m_interactionManager->SourceUpdated -= m_sourceUpdatedEventToken;
}

// Checks if the user performed an input gesture since the last call to this method.
// Allows the main update loop to check for asynchronous changes to the user
// input state.
SpatialInteractionSourceState^ SpatialInputHandler::CheckForInput()
{
	return m_sourceState;
}

void SpatialInputHandler::OnSourcePressed(
	SpatialInteractionManager^ sender,
	SpatialInteractionSourceEventArgs^ args)
{
	m_spatialInputState = eSISPressed;
	m_sourceState = args->State;
}

void SpatialInputHandler::OnSourceReleased(
	Windows::UI::Input::Spatial::SpatialInteractionManager^ sender,
	Windows::UI::Input::Spatial::SpatialInteractionSourceEventArgs^ args)
{
	m_spatialInputState = eSISReleased;
	m_sourceState = args->State;
}

void SpatialInputHandler::OnSourceUpdated(
	Windows::UI::Input::Spatial::SpatialInteractionManager^ sender,
	Windows::UI::Input::Spatial::SpatialInteractionSourceEventArgs^ args)
{
	m_spatialInputState = eSISMoved;
	m_sourceState = args->State;
}
