int main()
{
    int temporary = 1;
    
    auto foo = new function ()
    {
        return new function (int argument) { return argument; };
    };
    
    for (int i = 0; i < 1000000; ++ i)
        cout << foo()(temporary) << endl;
}
