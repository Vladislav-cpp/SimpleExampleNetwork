#include <iostream>

#define ASIO_STANDELONE
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

const std::string m_sFailConnect = "Failed to connect to sddress:";
const std::string m_sSuccessConnect = "Conected!";


std::vector<char> vBuffer(1024 * 20);



void GrabSomeData(asio::ip::tcp::socket& socket) {

	socket.async_read_some( asio::buffer(vBuffer), 
		[&](std::error_code ec, std::size_t lenght) {
			if(ec) return;

			std::cout << "\n\nRead " << lenght << " bytes\n\n";

			for(const auto c : vBuffer) std::cout << c;

			GrabSomeData(socket);
		}
	);

}

int main() {
	asio::error_code ec;

	// Create a "context" - essentially the platform specific anterface
	asio::io_context context;

	//Give some fake tasks to asio so the context does finish
	auto work = asio::make_work_guard(context);

	// Start the context
	std::thread thrContext = std::thread([&]() { context.run(); });
	
	// Get the address of somwehe we wish to connect to
	asio::ip::tcp::endpoint endpoint(asio::ip::make_address("", ec), 80);

	// Create a socket, the contex willdeliver the implementation 
	asio::ip::tcp::socket socket(context);

	// Tell socket to try and connect
	socket.connect(endpoint, ec);

	std::cout << ( ec ? m_sFailConnect + "\n" + ec.message() : m_sSuccessConnect ) << std::endl;

	if( socket.is_open() ) {
		
		GrabSomeData(socket);
		
		std::string sRequest = "yada_yada_yada";
		
		socket.write_some(asio::buffer(sRequest), ec);
		
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(2000ms);
		auto id = std::this_thread::get_id();
	}


	std::system("pause");
	return 0;
}