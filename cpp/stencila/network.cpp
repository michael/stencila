#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>

#include <stencila/network.hpp>
#include <stencila/string.hpp>

namespace Stencila {

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

Server::Server(void){
	// Initialise asynchronous IO
	server_.init_asio();
	// Set up handlers
	server_.set_open_handler(bind(&Server::open_,this,_1));
	server_.set_close_handler(bind(&Server::close_,this,_1));
	server_.set_http_handler(bind(&Server::http_,this,_1));
	server_.set_message_handler(bind(&Server::message_,this,_1,_2));
	// Only log some things. See
	//   http://www.zaphoyd.com/websocketpp/manual/reference/logging
	// for a full list
	server_.clear_access_channels(websocketpp::log::alevel::all);
	server_.set_access_channels(websocketpp::log::alevel::connect);
	server_.clear_error_channels(websocketpp::log::elevel::all);
	server_.set_error_channels(websocketpp::log::elevel::warn);
	server_.set_error_channels(websocketpp::log::elevel::rerror);
	server_.set_error_channels(websocketpp::log::elevel::fatal);
	// Log to files
	auto dir = boost::filesystem::temp_directory_path();
	dir /= "stencila/logs";
	if(not boost::filesystem::exists(dir)) boost::filesystem::create_directories(dir);
	access_log_.open((dir/"server-access.log").string());
	error_log_.open((dir/"server-error.log").string());
	server_.get_alog().set_ostream(&access_log_);
	server_.get_elog().set_ostream(&error_log_);
	// Allow reuse of address in case still in TIME_WAIT state
	// after previous unclean shutdown.
	// See http://hea-www.harvard.edu/~fine/Tech/addrinuse.html
	server_.set_reuse_addr(true);
	// Reset number of restarts
	restarts_ = 0;
}

std::string Server::url(void) const {
	return "http://localhost:"+string(port_);
}

void Server::start(void){
	try {
		server_.listen(port_);
		server_.start_accept();
		server_.run();
	} catch (std::exception const & e) {
        error_log_ << "[XXXX-XX-XX XX:XX:XX] [exception]" << e.what() << std::endl;
        restart_();
    } catch (...) {
        error_log_ << "[XXXX-XX-XX XX:XX:XX] [exception] Unknown exception" << std::endl;
        restart_();
    }
}

void Server::stop(void){
	server_.stop();
}

Server* server_instance_ = 0;
boost::thread* server_thread_ = 0;

std::string Server::startup(void) {
	if(not server_instance_){
		server_instance_ = new Server();
		server_thread_ = new boost::thread([](){
			server_instance_->start();
		});
	}
	return server_instance_->url();
}

void Server::shutdown(void) {
	if(server_instance_){
		server_instance_->stop();
		server_thread_->join();
		delete server_instance_;
		delete server_thread_;
	}
}

void Server::restart_(void){
	restarts_ += 1;
	if(restarts_<max_restarts_) start();
}

Server::Session& Server::session_(connection_hdl hdl) {
	auto i = sessions_.find(hdl);
	if(i==sessions_.end()) STENCILA_THROW(Exception,"No such session");
	return i->second;
}

std::string Server::decode_(const std::string& url) {
	std::string decoded = url;
	// Currently this only converts spaces.
	// More conversions will be required
	boost::replace_all(decoded,"%20"," ");
	return decoded;
}

std::string Server::address_(const std::string& path) {
	std::string address = path;
	if(address[0]=='/') address = address.substr(1);
	if(address[address.length()-1]=='/') address = address.substr(0,address.length()-1);
	return address;
}

void Server::open_(connection_hdl hdl) {
	server::connection_ptr connection = server_.get_con_from_hdl(hdl);
	std::string path = connection->get_resource();
	std::string address = address_(decode_(path));
	Session session = {address};
	sessions_[hdl] = session;
}

void Server::close_(connection_hdl hdl) {
	sessions_.erase(hdl);
}

void Server::http_(connection_hdl hdl) {
	// Get the connection 
	server::connection_ptr connection = server_.get_con_from_hdl(hdl);
	// Get the request resource and decode it
	// get_resource() returns "/" when there is no resource part in the URI
	// (i.e. if the URI is just http://localhost/)
	std::string resource = decode_(connection->get_resource());
	// Extract query
	std::string query;
	std::size_t found =  resource.find("?");
    if(found > 0){
    	query = resource.substr(found+1);
    	resource = resource.substr(0,found);
    }
	// Get request method
	auto request = connection->get_request();
	std::string method = request.get_method();
	// Get the remote address
	std::string remote = connection->get_remote_endpoint();
	// Response variables
	http::status_code::value status = http::status_code::ok;
	std::string content;
	try {
		if(resource=="/"){
			content = Component::index();
		} 
		else if(resource=="/extras"){
			content = Component::extras();
		}
		else {
			// This server handles two types of requents for Components:
			// (1) "Dynamic" requests where the component is loaded into
			// memory (if not already) and (2) Static requests for component
			// files
			// Static requests are indicated by a "." anywhere in the url
			bool dynamic = resource.find(".")==std::string::npos;
			if(dynamic){
				// Dynamic request
				// 
				// Components must be served with a trailing slash so that relative links work.
				// For example, if a stencil with address "a/b/c" is served with the url "/a/b/c/"
				// then a relative link within that stencil to an image "1.png" will resolved to "/a/b/c/1.png" (which
				// is what we want) but without the trailing slash will be resolved to "/a/b/1.png" (which 
				// will cause a 404 error). 
				// So, if no trailing slash, then redirect...
				if(resource[resource.length()-1]!='/'){
					status = http::status_code::moved_permanently;
					// Use full URI for redirection because multiple leading slashes can get
					// squashed up otherwise
					auto uri = url()+resource+"/";
					connection->append_header("Location",uri);
				}
				// Provide the page content
				else {
					content = Component::page(address_(resource));
				}
			} else {
				// Static request
				std::string address = address_(resource);
				std::string path = Component::locate(address);
				if(path.length()==0){
					// 404: not found
					status = http::status_code::not_found;
					content = "Not found\n address: "+address;
				} else {
					// Check to see if this is a directory
					if(boost::filesystem::is_directory(path)){
						// 403: forbidden
						status = http::status_code::forbidden;
						content = "Directory access is forbidden\n  path: "+path;		
					}
					else {
						std::ifstream file(path);
						if(not file.good()){
							// 500 : internal server error
							status = http::status_code::internal_server_error;
							content = "File error\n  path: "+path;
						} else {
							// Read file into content string
							// There may be a [more efficient way to read a file into a string](
							// http://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring)
							std::string file_content(
								(std::istreambuf_iterator<char>(file)),
								(std::istreambuf_iterator<char>())
							);
							content = file_content;
							// Determine and set the "Content-Type" header
							std::string content_type;
							std::string extension = boost::filesystem::path(path).extension().string();
							if(extension==".txt") content_type = "text/plain";
							else if(extension==".css") content_type = "text/css";
							else if(extension==".html") content_type = "text/html";
							else if(extension==".png") content_type = "image/png";
							else if(extension==".jpg" || extension==".jpeg") content_type = "image/jpg";
							else if(extension==".svg") content_type = "image/svg+xml";
							else if(extension==".js") content_type = "application/javascript";
							else if(extension==".woff") content_type = "application/font-wof";
							else if(extension==".tff") content_type = "application/font-ttf";
							connection->append_header("Content-Type",content_type);
						}
					}
				}
			}
		}
	}
	catch(const std::exception& e){
		status = http::status_code::internal_server_error;
		content = std::string(e.what());
	}
	catch(...){
		status = http::status_code::internal_server_error;
		content = "Unknown exception";			
	}
	// Replace the WebSocket++ "Server" header
	connection->replace_header("Server","Stencila embedded");
	// Set status and content
	connection->set_status(status);
	connection->set_body(content);
}

/**
 * Handle a websocket message
 * 
 * @param hdl Connection handle
 * @param msg Message pointer
 */
void Server::message_(connection_hdl hdl, server::message_ptr msg) {
	std::string response;
	try {
		Session session = session_(hdl);
		std::string request = msg->get_payload();
		response = Component::message(session.address,request);
	}
	// `Component::message()` should handle most exceptions and return a WAMP
	// ERROR message. If for some reason that does not happen, the following returns
	// a plain text error message...
	catch(const std::exception& e){
		response = "Internal server error : " + std::string(e.what());
	}
	catch(...){
		response = "Internal server error : unknown exception";			
	}
	server_.send(hdl,response,opcode::text);
}

} // namespace Stencila
