int main()
{
    int temporary = 1;
    
    auto foo = function ()
    {
        return function (int argument) { return argument; };
    };
    
    for (int i = 0; i < 1000000; ++ i)
        cout << (foo()(temporary) * i) << endl;
}
