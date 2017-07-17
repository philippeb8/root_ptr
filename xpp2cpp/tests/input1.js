{
    var temporary = new 0;
    
    var bar = function ()
    {
        return new 10;
    };
    
    var foo = function ()
    {
        var object = new 10;
        return function(argument) { return new 10; };
    };
    
    for (int i = 0; i < 1000000; ++ i)
        cout << (foo())(temporary) << endl;

    //for (var i = 0; i < 1000000; ++ i)
    //    console.log((foo())(temporary));
}
