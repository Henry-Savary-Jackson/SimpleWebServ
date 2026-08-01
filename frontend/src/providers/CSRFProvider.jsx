import { createContext, useEffect, useState } from "react";
import { getCSRF } from "../utils/RequestUtils"

var CSRFContext = createContext("")


function CSRFProvider({children}) {
    let [csrf, setCSRF] = useState("")

    useEffect(() => {
        async function action() {
            try {
                setCSRF(await getCSRF())
            } catch (e) {
                console.error("failed to get csrf")
            }
        }
    }, []);

    return <CSRFContext.Provider value={csrf}>
        {children}
    </CSRFContext.Provider>

}

export  {CSRFProvider, CSRFContext};
