#include <iostream>
#include <string>
#include <functional>
#include <vector>
#include <cassert>

#define BOOST_RESULT_OF_USE_DECLTYPE
#include <boost/utility/result_of.hpp>

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/void.hpp>
#include <boost/fusion/container.hpp>
#include <boost/fusion/functional.hpp>
#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/include/mpl.hpp>
#include <boost/fusion/include/as_vector.hpp>
#include <boost/function_types/function_type.hpp>

using namespace boost;
using boost::mpl::void_;

template<typename T>
class c {
public:
	T operator()(const std::string &s, bool *success) const {
		try {
			if ( success )
				*success = true;
			return lexical_cast<T>(s);
		} catch ( ... ) {
			if ( success )
				*success = false;
		}
		T a;
		return a;
	}

	std::string operator()(const T& t) const {
		return lexical_cast<std::string>(t);
	}
};

enum Method { Method_GET };

template<typename It>
class PathVisitor
{
	It *i;
	const It end;
	bool *m_success;
public:
	PathVisitor(It *i, const It &end, bool *success) : i(i), end(end), m_success(success) {} 

	bool isSuccess() const { return *m_success && *i == end; }
	

	void_ operator()(std::string t) const {
		void_ v;

		if ( !*m_success ) return v;
		if ( *i == end ) return v;

		*m_success = (**i == t);
		++(*i);
		return v;
	}

	template<typename T>
	T operator()(const c<T>& t) const {
		T tmp;
		if ( !*m_success ) return tmp;
		if ( *i == end ) return tmp;
		
		const T &r = t(**i, m_success);
		++(*i);
		return r;
	}
};

template<typename Args>
struct URLBuilder {
	Args args;

	URLBuilder(const Args &args) : args(args) {}

	template<int N>
	fusion::vector< std::string, mpl::int_<N> > operator()(const fusion::vector< std::string, mpl::int_<N> > &r,
		const std::string &i) const {
		
		return fusion::vector< std::string, mpl::int_<N> >(
			fusion::at_c<0>(r)+"/"+i,
			mpl::int_<N>());
	}

	template<typename T, int N>
	fusion::vector< std::string, mpl::int_<N+1> > operator()(const fusion::vector< std::string, mpl::int_<N> > &r,
		const c<T> &i) const {
		return fusion::vector< std::string, mpl::int_<N+1> >(
			fusion::at_c<0>(r)+"/"+i(fusion::at_c<N>(args)),
			mpl::int_<N+1>() );
	}
};

class FixedRoute {
public:
	virtual bool operator()(const std::string &p) const = 0;
};

template<typename T, typename S>
class FixedRouteImpl : public FixedRoute {
public:
	typedef S ComponentType;
	typedef typename fusion::result_of::as_vector< typename mpl::pop_front<T>::type >::type ParamTuple;
	typedef std::function< typename function_types::function_type<T>::type > HandlerType;
private:
	const HandlerType handler;
	const ComponentType components;
public:
	FixedRouteImpl(HandlerType h, const ComponentType &c) : handler(h), components(c) {}

	std::string str(const ParamTuple &c) const {
		return fusion::at_c<0>(fusion::fold(
			components, fusion::vector< std::string, mpl::int_<0> >() ,URLBuilder<ParamTuple>(c)
			));
	}

	virtual bool operator()(const std::string &p) const {
		typedef tokenizer<char_separator<char> > Tok;
		Tok tok(p, char_separator<char>("/"));
		
		/*
		std::cout << "Tokenized\n\t";
		for ( auto i = tok.begin(); i != tok.end(); ++i )
			std::cout << "<" << *i << "> ";
		std::cout << "\n";
		*/

		auto t = tok.begin();
		bool b = true;
		PathVisitor<Tok::iterator> v(&t, tok.end(), &b);

		auto params = fusion::as_vector(fusion::transform(components, v));

		if ( v.isSuccess() ) {
			fusion::invoke_procedure(handler, fusion::remove_if< mpl::is_void_<mpl::_> >(params) );
		}

		return v.isSuccess();
	}
};


std::vector<FixedRoute*> m_routes;

template<typename T, typename S>
class Route {
	const Method m;
	const S components;
public:
	typedef typename fusion::result_of::as_vector<typename fusion::result_of::reverse<
		const typename fusion::result_of::as_vector< S >::type>::type >::type FixedRouteComponentType;

	Route(Method m, const S &components) : m(m), components(components) {}

	template<typename N>
	Route< typename mpl::push_back<T,N>::type, fusion::cons< c<N>,S> > operator/(const c<N> &component) const {
		return Route< typename mpl::push_back<T,N>::type, fusion::cons< c<N>,S> >(m, fusion::cons< c<N>, S>(component, components));
	}

	FixedRouteImpl<T, FixedRouteComponentType>& operator=(const std::function< typename function_types::function_type<T>::type> &handler) const {
		auto r = new FixedRouteImpl<T, FixedRouteComponentType>(handler, fusion::reverse(fusion::as_vector(components)));
		m_routes.push_back(r);
		return *r;
	}
};

const struct Root { int dummy; Root() {} } root;

class RouteMethod {
	const Method m;
public:
	RouteMethod(Method m) : m(m) {}

	Route< mpl::vector<void>, fusion::cons<std::string> > operator/(const std::string &s) const {
		return Route< mpl::vector<void>, fusion::cons<std::string> >(m, fusion::cons<std::string>(s));
	}

	Route< mpl::vector<void>, fusion::nil > operator/(const Root &s) const {
		return Route< mpl::vector<void>, fusion::nil >(m, fusion::nil() );
	}
	
	template<typename T>
	Route< mpl::vector<void,T>, fusion::cons< c<T> > > operator/(const c<T> &component) const {
		return Route< mpl::vector<void,T>, fusion::cons< c<T> > >(m, fusion::cons< c<T> >(component));
	}
};
RouteMethod GET(Method_GET);


bool route(const std::string &r) {
	for (auto i = 0; i < m_routes.size(); ++i) {
		if ( (*m_routes[i])(r) ) return true;
	}
	return false;
}

////////////////////////
///
///		Test
///
/////////////////////////

void handler0() {
	std::cout << "handler0 called\n";
}

void handler1(int param) {
	std::cout << "handler1(" << param <<") called\n";
}

void handler2(int param0, const std::string &param1) {
	std::cout << "handler2("  << param0 << ',' << param1 << ")called\n";
}

int main () {
	GET / root = handler0;
	GET / "aya" = handler0;
	GET / "comp" / c<int>() = handler1;
	auto r = (GET / "comp" / c<int>() / c<std::string>() = handler2);

	route("/");
	route("/aya");
	route("/comp/1");
	route("/comp/20");
	route("/comp/3/wee");

	std::cout << r.str(fusion::make_vector(10,std::string("hello"))) << '\n';

	return 0;
}



