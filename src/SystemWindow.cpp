#include "SystemWindow.hpp"
#include <SDL.h>


namespace KK
{

namespace
{

SystemEventTypes::KeyCode TranslateKey(const SDL_Scancode scan_code)
{
	using KeyCode= SystemEventTypes::KeyCode;

	switch( scan_code )
	{
	case SDL_SCANCODE_ESCAPE: return KeyCode::Escape;
	case SDL_SCANCODE_RETURN: return KeyCode::Enter;
	case SDL_SCANCODE_SPACE: return KeyCode::Space;
	case SDL_SCANCODE_BACKSPACE: return KeyCode::Backspace;
	case SDL_SCANCODE_GRAVE: return KeyCode::BackQuote;
	case SDL_SCANCODE_TAB: return KeyCode::Tab;

	case SDL_SCANCODE_PAGEUP: return KeyCode::PageUp;
	case SDL_SCANCODE_PAGEDOWN: return KeyCode::PageDown;

	case SDL_SCANCODE_RIGHT: return KeyCode::Right;
	case SDL_SCANCODE_LEFT: return KeyCode::Left;
	case SDL_SCANCODE_DOWN: return KeyCode::Down;
	case SDL_SCANCODE_UP: return KeyCode::Up;

	case SDL_SCANCODE_MINUS: return KeyCode::Minus;
	case SDL_SCANCODE_EQUALS: return KeyCode::Equals;

	case SDL_SCANCODE_LEFTBRACKET: return KeyCode::SquareBrackretLeft;
	case SDL_SCANCODE_RIGHTBRACKET: return KeyCode::SquareBrackretRight;

	case SDL_SCANCODE_SEMICOLON: return KeyCode::Semicolon;
	case SDL_SCANCODE_APOSTROPHE: return KeyCode::Apostrophe;
	case SDL_SCANCODE_BACKSLASH: return KeyCode::BackSlash;

	case SDL_SCANCODE_COMMA: return KeyCode::Comma;
	case SDL_SCANCODE_PERIOD: return KeyCode::Period;
	case SDL_SCANCODE_SLASH: return KeyCode::Slash;

	case SDL_SCANCODE_PAUSE: return KeyCode::Pause;

	default:
		if( scan_code >= SDL_SCANCODE_A && scan_code <= SDL_SCANCODE_Z )
			return KeyCode( int(KeyCode::A) + (scan_code - SDL_SCANCODE_A) );
		if( scan_code >= SDL_SCANCODE_1 && scan_code <= SDL_SCANCODE_9 )
			return KeyCode( int(KeyCode::K1) + (scan_code - SDL_SCANCODE_1) );
		if( scan_code == SDL_SCANCODE_0 )
			return KeyCode::K0;
		if( scan_code >= SDL_SCANCODE_F1 && scan_code <= SDL_SCANCODE_F12 )
			return KeyCode( int(KeyCode::F1) + (scan_code - SDL_SCANCODE_F1) );
	};

	return KeyCode::Unknown;
}

SystemEventTypes::ButtonCode TranslateMouseButton(const Uint8 button)
{
	using ButtonCode= SystemEventTypes::ButtonCode;
	switch(button)
	{
	case SDL_BUTTON_LEFT: return ButtonCode::Left;
	case SDL_BUTTON_RIGHT: return ButtonCode::Right;
	case SDL_BUTTON_MIDDLE: return ButtonCode::Middle;
	};

	return ButtonCode::Unknown;
}

uint16_t TranslateKeyModifiers(const Uint16 modifiers)
{
	uint16_t result= 0u;

	if( ( modifiers & ( KMOD_LSHIFT | KMOD_LSHIFT ) ) != 0u )
		result|= SystemEventTypes::KeyModifiers::Shift;

	if( ( modifiers & ( KMOD_LCTRL | KMOD_RCTRL ) ) != 0u )
		result|= SystemEventTypes::KeyModifiers::Control;

	if( ( modifiers & ( KMOD_RALT | KMOD_LALT ) ) != 0u )
		result|= SystemEventTypes::KeyModifiers::Alt;

	if( ( modifiers &  KMOD_CAPS ) != 0u )
		result|= SystemEventTypes::KeyModifiers::Caps;

	return result;
}

} // namespace

SystemWindow::SystemWindow()
{
	// TODO - check errors.
	SDL_Init(SDL_INIT_VIDEO);

	window_=
		SDL_CreateWindow(
			"Klassenkampf",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			800, 600,
			SDL_WINDOW_SHOWN);
}

SystemWindow::~SystemWindow()
{
	SDL_DestroyWindow(window_);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

SystemEvents SystemWindow::ProcessEvents()
{
	SystemEvents result_events;

	SDL_Event event;
	while( SDL_PollEvent(&event) )
	{
		switch(event.type)
		{
		case SDL_WINDOWEVENT:
			if(event.window.event == SDL_WINDOWEVENT_CLOSE)
				result_events.emplace_back(SystemEventTypes::QuitEvent());
			break;

		case SDL_QUIT:
			result_events.emplace_back(SystemEventTypes::QuitEvent());
			break;

		case SDL_KEYUP:
		case SDL_KEYDOWN:
			{
				SystemEventTypes::KeyEvent key_event;
				key_event.key_code= TranslateKey(event.key.keysym.scancode);
				key_event.pressed= event.type == SDL_KEYUP ? false : true;
				key_event.modifiers= TranslateKeyModifiers(event.key.keysym.mod);
				result_events.emplace_back(key_event);
			}
			break;

		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
			{
				SystemEventTypes::MouseKeyEvent mouse_event;
				mouse_event.mouse_button= TranslateMouseButton(event.button.button);
				mouse_event.x= uint16_t(event.button.x);
				mouse_event.y= uint16_t(event.button.y);
				mouse_event.pressed= event.type == SDL_MOUSEBUTTONUP ? false : true;
				result_events.emplace_back(mouse_event);
			}
			break;

		case SDL_MOUSEMOTION:
			{
				SystemEventTypes::MouseMoveEvent mouse_event;
				mouse_event.dx= uint16_t(event.motion.xrel);
				mouse_event.dy= uint16_t(event.motion.yrel);
				result_events.emplace_back(mouse_event);
			}
			break;

		case SDL_TEXTINPUT:
			// Accept only visible ASCII
			if( event.text.text[0] >= 32 )
			{
				SystemEventTypes::CharInputEvent char_event;
				char_event.ch= event.text.text[0];
				result_events.emplace_back(char_event);
			}
			break;

		// TODO - fill other events here

		default:
			break;
		};
	}

	return result_events;
}

void SystemWindow::BeginFrame()
{
}

void SystemWindow::EndFrame()
{
	SDL_UpdateWindowSurface(window_);
}

} // namespace KK