{
    var temporary = new int(1);
    
    var bar = function ()
    {
        return new int(2);
    };
    
    var foo = function ()
    {
        return function(argument) { return argument; };
    };
    
    for (int i = 0; i < 1000000; ++ i)
        cout << ((foo())(temporary) + bar()) << endl;
}
