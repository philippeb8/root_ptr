class Document
{
    //auto head = new int(0);
    //auto tail = new int(0);

    Document() 
    {
        // cycle
        //head = tail; 
        //tail = head; 
        cout << __PRETTY_FUNCTION__ << endl; 
    }
    ~Document() { cout << __PRETTY_FUNCTION__ << endl; }
    
    auto foo = function (int argument) { cout << __PRETTY_FUNCTION__ << endl; return argument; };
};

int main()
{
    auto temporary = 1;
    
    auto document = new Document();
    document.foo(temporary);
    
    auto bar = function ()
    {
        auto document = new Document();
        
        return 2;
    };
    
    cout << 1 << endl;
    bar();
    cout << 2 << endl;
}
