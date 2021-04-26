#include <type_traits>

template <int...>
struct Vector;

template <int I, int... L>
struct Vector<I, L...>
{
	int val = I;
};

template <int In, typename Vector>
struct AddType;

template <int I1, int... In>
struct AddType<I1, Vector<In...>>
{
	using type = Vector<I1, In...>;
};

template<typename Vector>
struct RemoveDuplicates
{
	using type = Vector;
};

template <int I1, int... In>
struct RemoveDuplicates<Vector<I1, I1, In...>>
{
	using type = typename RemoveDuplicates<Vector<I1, In...>>::type;
};

template <int I1, int... In>
struct RemoveDuplicates<Vector<I1, In...>>
{
	using type = typename AddType<I1, typename RemoveDuplicates<Vector<In...>>::type>::type;
};

template <int... I>
struct RemoveDuplicates<Vector<I...>>
{
	using type = Vector<I...>;
};

int main()
{
	static_assert(
		std::is_same<
			RemoveDuplicates<
				Vector<1, 2, 2, 3, 4, 4>
			>::type,
			Vector<1, 2, 3, 4>
		>::value //This should be true
	);

	static_assert(
		std::is_same<
			RemoveDuplicates<
				Vector<1, 2, 2, 3, 4, 4, 1, 5>
			>::type,
			Vector<1, 2, 3, 4, 1, 5>
		>::value //This should be true
	);

	static_assert(
		std::is_same<
			RemoveDuplicates<
				Vector<1>
			>::type,
			RemoveDuplicates<
				Vector<1>
			>::type
		>::value //This should be true
	);
	return 0;
}
