# ScreenStreamMJPEGPlugin

**Version:** 1.0.0 
**Engine Version:** Unreal Engine 5.3.2  
**License:** MIT License

## Description

ScreenStreamMJPEGPlugin is a plugin for Unreal Engine that enables real-time screen capture and streaming of MJPEG video over HTTP. It allows users to capture their screen and serve the captured video stream to multiple clients over a network.

## Features

- Real-time screen capture.
- MJPEG video streaming over HTTP.
- Compatible with Unreal Engine 5.3.2.

## Based on

- [UnrealImageCapture](https://github.com/TimmHess/UnrealImageCapture): Used for screen capturing functionality.
- [cpp-mjpeg-streamer](https://github.com/nadjieb/cpp-mjpeg-streamer): C++ code for MJPEG streaming server.

## Installation

1. Clone or download this repository.
2. Build and copy resulting `ScreenStreamMJPEGPlugin` folder into the `Plugins` directory of your Unreal Engine project.
3. Enable the plugin in your Unreal Engine project settings.

## Usage

1. Import the plugin into your Unreal Engine project.
2. Configure the screen capture settings, including, resolution, HTTP port for MJPEG server etc.
3. Start your application to begin streaming the screen capture.
4. Access the MJPEG stream from any HTTP client by connecting to the appropriate URL.

## Contributing

Contributions are welcome! If you encounter any issues or have suggestions for improvements, please open an issue or submit a pull request on GitHub.

## License

This project is licensed under the [MIT License](LICENSE).

## Support

For support or inquiries, please contact [Nikita Petrov](mailto:nikitapetroff@gmail.com).
